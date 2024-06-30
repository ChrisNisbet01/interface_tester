from __future__ import annotations
import json
import queue
import subprocess
import threading
import time
from dataclasses import dataclass
from logging import Logger
from subprocess import Popen
from threading import Thread
from typing import Any

from fixtures.waiter import Waiter

_UBUSD_APPLICATION = "ubusd"


class Ubus:
    _socket_path: str
    _ubusd_process: Popen | None = None

    def __init__(self, socket_path: str):
        self._socket_path = socket_path

    def start(self) -> None:
        self._ubusd_process = subprocess.Popen([_UBUSD_APPLICATION, "-s", self._socket_path])

    def stop(self) -> None:
        if self._ubusd_process:
            self._ubusd_process.terminate()

    @property
    def socket_path(self):
        return self._socket_path

    def __enter__(self) -> Ubus:
        self.start()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb) -> None:
        self.stop()

    def send_event(self, event_name: str, event_data: dict[str, Any]) -> (int, str):
        event_data_json = json.dumps(event_data)
        result = subprocess.run(
            ["ubus", "-s", self.socket_path, "send", event_name, event_data_json],
            capture_output=True,
            text=True,
        )

        return result.returncode, result.stdout

@dataclass
class UbusEvent:
    timestamp: float
    data: dict[str, any]


class UbusListener:
    _ubusd: Ubus
    _process: Popen | None = None
    _read_thread: Thread | None = None
    _events: queue.Queue
    _log: Logger
    _waiter: Waiter

    def __init__(self, ubusd: Ubus, logger: Logger, waiter: Waiter) -> None:
        self._ubusd = ubusd
        self._log = logger
        self._waiter = waiter
        self._events = queue.Queue()

    def __enter__(self) -> UbusListener:
        return self

    def __exit__(self, exc_type, exc_val, exc_tb) -> None:
        self.stop_listener()

    def stop_listener(self) -> None:
        if self._process:
            self._process.terminate()
        if self._read_thread:
            self._read_thread.join()

    def _start_listener(self) -> Popen:
        self._events = queue.Queue()
        self.stop_listener()
        # Start a process that outputs JSON objects line by line
        process = Popen(
            ['ubus', '-s', self._ubusd.socket_path, "listen"],
            stdout=subprocess.PIPE,
            text=True  # To get string output, not bytes
        )
        return process

    def _read_json_stream(self, process: Popen):
        buffer = ""
        for line in process.stdout:
            buffer += line
            while True:
                try:
                    # Try to parse the buffer as JSON
                    event_data, index = json.JSONDecoder().raw_decode(buffer)
                    # Process the JSON object (replace this with your own processing logic)
                    # Remove the parsed JSON object from the buffer
                    buffer = buffer[index:].lstrip()
                except json.JSONDecodeError:
                    # If we reach here, it means we don't have a complete JSON object in the buffer yet
                    break
                else:
                    self._log.debug(f"event: {event_data}")
                    self._events.put(UbusEvent(timestamp=time.perf_counter(), data=event_data))

    def listen(self) -> None:
        self._process = self._start_listener()
        self._read_thread = threading.Thread(target=self._read_json_stream, args=(self._process,))
        self._read_thread.start()

    @staticmethod
    def _dict_contains_all(sub_dict: dict, main_dict: dict) -> bool:
        for key, value in sub_dict.items():
            if key not in main_dict or main_dict[key] != value:
                return False
        return True

    def wait_for_event(self, event_name: str, data: dict[str, Any], timeout: float):
        def have_event() -> UbusEvent | bool:
            while True:
                try:
                    event = self._events.get(block=False)
                except queue.Empty:
                    return False
                event_data = event.data
                if event_name not in event_data.keys():
                    continue
                if not isinstance(event_data[event_name], dict):
                    continue
                if self._dict_contains_all(data, event_data[event_name]):
                    return event
        return self._waiter.wait_for(have_event, timeout, interval=0.1, description=f"event: {event_name} with: {data}")
