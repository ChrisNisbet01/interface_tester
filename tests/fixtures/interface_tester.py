from __future__ import annotations

import dataclasses
import json
import logging
import subprocess
import threading
from dataclasses import dataclass
from subprocess import Popen
from typing import Any

from fixtures.ubus import Ubus
from enum import StrEnum


class SuccessCondition(StrEnum):
    ALL_TESTS_MUST_PASS = 'all_tests_must_pass'
    ONE_TEST_MUST_PASS = 'one_test_must_pass'


@dataclass
class IfaceTesterTaskConfig:
    executable: str
    label: str
    response_timeout_secs: int
    params: dict[str, Any] = dataclasses.field(default_factory=lambda: {})


@dataclass
class IfaceTesterTestConfig:
    executable: str
    label: str
    params: dict[str, Any] = dataclasses.field(default_factory=lambda: {})
    response_timeout_secs: int = 0


@dataclass
class IfaceTesterConfig:
    tests: list[IfaceTesterTestConfig]
    recovery_tasks: list[IfaceTesterTaskConfig] = dataclasses.field(default_factory=lambda: [])
    success_condition: SuccessCondition = SuccessCondition.ALL_TESTS_MUST_PASS
    settling_delay_secs: int = 1
    passing_interval_secs: int = 10
    failing_interval_secs: int = 4
    pass_threshold: int = 1
    fail_threshold: int = 1
    response_timeout_secs: int = 5
    failing_tests_metrics_increase: int = 0


@dataclass
class IfaceTesterInterfaceConfig:
    name: str
    config: IfaceTesterConfig


class InterfaceTester:
    _exe_path: str
    _log: logging.Logger
    _ubusd: Ubus
    _process: Popen | None = None
    _read_thread: threading.Thread | None = None

    def __init__(self, exe_path: str, ubusd: Ubus, logger: logging.Logger):
        self._exe_path = exe_path
        self._ubusd = ubusd
        self._log = logger

    def __enter__(self) -> InterfaceTester:
        return self

    def __exit__(self, exc_type, exc_val, exc_tb) -> None:
        self.stop()

    def _start_tester(
            self, config: str | None = None, test_dir: str | None = None, tasks_dir: str | None = None
    ) -> Popen:
        args = [
                self._exe_path,
                "-s",
                self._ubusd.socket_path,
            ]
        if config:
            args.extend(["-c", config])
        if test_dir:
            args.extend(["-S", test_dir])
        if tasks_dir:
            args.extend(["-r", tasks_dir])
        process = Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        return process

    def _read_tester(self, process: Popen) -> None:
        for line in process.stderr:
            self._log.debug(f"tester: {line}")

    def start(self, config: str | None = None, test_dir: str | None = None, tasks_dir: str | None = None) -> None:
        self.stop()
        self._process = self._start_tester(config=config, test_dir=test_dir, tasks_dir=tasks_dir)
        self._read_thread = threading.Thread(target=self._read_tester, args=(self._process,))
        self._read_thread.start()

    def stop(self):
        if self._process:
            self._process.terminate()
        if self._read_thread:
            self._read_thread.join()

    def load_config(self, configs: list[IfaceTesterInterfaceConfig]) -> str:
        # Convert to a dict
        as_dict = {"interfaces": {}}
        for config in configs:
            iface_name = config.name
            iface_config = dataclasses.asdict(config.config)
            as_dict["interfaces"][iface_name] = iface_config
        as_json = json.dumps(as_dict)
        self._log.debug(f"config: {as_json}")
        result = subprocess.run(
            ["ubus", "-s", self._ubusd.socket_path, "call", "interface.tester", "config", as_json],
            capture_output=True,
            text=True,
        )
        assert result.returncode == 0, "Failed to load configuration into interface tester"
        return result.returncode, result.stdout
