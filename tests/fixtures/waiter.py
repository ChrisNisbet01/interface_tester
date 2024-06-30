from __future__ import annotations

import time
from logging import Logger
from typing import Any, Callable

WaiterPredicate: Callable[..., Any]


class TimeoutException(Exception):
    ...


class Waiter:
    _log: Logger

    def __init__(self, logger: Logger) -> None:
        self._log = logger

    def __enter__(self) -> Waiter:
        return self

    def __exit__(self, exc_type, exc_val, exc_tb) -> None:
        return

    def wait_for(
            self,
            predicate: WaiterPredicate,
            timeout: float | int,
            interval: float | int = 1,
            description: str = "",
            **kwargs,
    ) -> Any:
        start_time = time.time()
        end_time = start_time + timeout
        while time.time() - end_time <= 0:
            result = predicate(**kwargs)
            if result:
                return result
            time.sleep(interval)

        message = f"timeout waiting {timeout} seconds for: {description}"
        self._log.debug(message)
        raise TimeoutException(message)
