from _pytest.config import Config
from _pytest.fixtures import fixture
from typing import Generator
from fixtures.interface_tester import InterfaceTester
from fixtures.ubus import Ubus, UbusListener
from fixtures.waiter import Waiter
import logging

_UBUSD_SOCKET_PATH = "/tmp/interface_tester_ubusd.sock"


def pytest_addoption(parser):
    parser.addoption("--tester", action="store")
    parser.addoption("--config", action="store")
    parser.addoption("--tests", action="store")
    parser.addoption("--tasks", action="store")


@fixture(scope="session")
def logger() -> logging.Logger:
    return logging.getLogger("interface_tester")


@fixture(scope="session")
def waiter(logger: logging.Logger) -> Generator[Waiter, None, None]:
    with Waiter(logger) as w:
        yield w


@fixture(scope="session")
def ubusd() -> Generator[Ubus, None, None]:
    with Ubus(_UBUSD_SOCKET_PATH) as u:
        yield u


@fixture()
def ubus_listener(ubusd: Ubus, logger: logging.Logger, waiter: waiter) -> Generator[UbusListener, None, None]:
    with UbusListener(ubusd, logger, waiter) as ul:
        yield ul


@fixture()
def interface_tester(
        pytestconfig: Config, ubusd: Ubus, logger: logging.Logger
) -> Generator[InterfaceTester, None, None]:
    with InterfaceTester(pytestconfig.getoption("tester"), ubusd, logger) as tester:
        yield tester
