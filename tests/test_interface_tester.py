import time

from _pytest.config import Config
from fixtures.interface_tester import InterfaceTester, IfaceTesterInterfaceConfig, \
    IfaceTesterTestConfig, IfaceTesterConfig, SuccessCondition
from fixtures.ubus import UbusListener, Ubus


def test_interface_tester_up_down_events(ubus_listener: UbusListener, interface_tester: InterfaceTester) -> None:
    ubus_listener.listen()
    interface_tester.start()
    # It might take a little while for the application to start up, so by waiting for the up event to occur we can
    # be more sure that the down event also occurs when the application is stopped.
    ubus_listener.wait_for_event("interface.tester", {"state": "up"}, 5)
    interface_tester.stop()
    ubus_listener.wait_for_event("interface.tester", {"state": "down"}, 5)


def test_interface_tester_single_passing_test_all_tests_must_pass_passes_test_run(
    interface_tester: InterfaceTester, pytestconfig: Config, ubus_listener: UbusListener, ubusd: Ubus
) -> None:
    ubus_listener.listen()
    interface_tester.start(
        pytestconfig.getoption("config"), pytestconfig.getoption("tests"), pytestconfig.getoption("tasks")
    )
    interface_name = "wan"
    ubus_listener.wait_for_event("interface.tester", {"state": "up"}, 5)
    config = IfaceTesterInterfaceConfig(
        name=interface_name,
        config=IfaceTesterConfig(
            tests=[IfaceTesterTestConfig(executable="passing_test", label="Passing test")]
        ),
    )
    interface_tester.load_config([config])

    ubusd.send_event("interface.state", {"state": "ifup", "interface": interface_name})
    ubus_listener.wait_for_event(
        "interface.tester.test_run", {"result": "pass", "interface": interface_name}, 10
    )


def test_interface_tester_multiple_passing_tests_all_tests_must_pass_passes_test_run(
    interface_tester: InterfaceTester, pytestconfig: Config, ubus_listener: UbusListener, ubusd: Ubus
) -> None:
    ubus_listener.listen()
    interface_tester.start(
        pytestconfig.getoption("config"), pytestconfig.getoption("tests"), pytestconfig.getoption("tasks")
    )
    interface_name = "wan"
    ubus_listener.wait_for_event("interface.tester", {"state": "up"}, 5)
    config = IfaceTesterInterfaceConfig(
        name=interface_name,
        config=IfaceTesterConfig(
            tests=[
                IfaceTesterTestConfig(executable="passing_test", label="Passing test #1"),
                IfaceTesterTestConfig(executable="passing_test", label="Passing test #2"),
            ]
        ),
    )
    interface_tester.load_config([config])

    ubusd.send_event("interface.state", {"state": "ifup", "interface": interface_name})
    ubus_listener.wait_for_event(
        "interface.tester.test_run", {"result": "pass", "interface": interface_name}, 10
    )


def test_interface_tester_single_failing_test_all_tests_must_pass_fails_test_run(
    interface_tester: InterfaceTester, pytestconfig: Config, ubus_listener: UbusListener, ubusd: Ubus
) -> None:
    ubus_listener.listen()
    interface_tester.start(
        pytestconfig.getoption("config"), pytestconfig.getoption("tests"), pytestconfig.getoption("tasks")
    )
    interface_name = "wan"
    ubus_listener.wait_for_event("interface.tester", {"state": "up"}, 5)
    config = IfaceTesterInterfaceConfig(
        name=interface_name,
        config=IfaceTesterConfig(
            tests=[IfaceTesterTestConfig(executable="failing_test", label="Failing test")]
        ),
    )
    interface_tester.load_config([config])

    ubusd.send_event("interface.state", {"state": "ifup", "interface": interface_name})
    start_time = time.perf_counter()
    fail_event = ubus_listener.wait_for_event(
        "interface.tester.test_run", {"result": "fail", "interface": interface_name}, 10
    )
    time_allowance_for_test_secs = 1
    seconds_to_fail = fail_event.timestamp - start_time
    assert seconds_to_fail < config.config.settling_delay_secs + time_allowance_for_test_secs, \
        f"took too long for test to fail ({seconds_to_fail})"
    broken_event = ubus_listener.wait_for_event(
        "interface.tester.operational", {"is_operational": False, "interface": interface_name}, 1
    )
    assert broken_event.timestamp - fail_event.timestamp < 1, \
        "broken event should occur directly after first test run failure"


def test_interface_tester_single_passing_test_that_takes_too_long_fails_test_run(
    interface_tester: InterfaceTester, pytestconfig: Config, ubus_listener: UbusListener, ubusd: Ubus
) -> None:
    ubus_listener.listen()
    interface_tester.start(
        pytestconfig.getoption("config"), pytestconfig.getoption("tests"), pytestconfig.getoption("tasks")
    )
    interface_name = "wan"
    ubus_listener.wait_for_event("interface.tester", {"state": "up"}, 5)
    response_timeout_secs = 1
    config = IfaceTesterInterfaceConfig(
        name=interface_name,
        config=IfaceTesterConfig(
            response_timeout_secs=response_timeout_secs,
            tests=[
                IfaceTesterTestConfig(
                    executable="passing_test",
                    label="passing test that takes too long",
                    params={"sleep": response_timeout_secs + 1},
                )
            ]
        ),
    )

    interface_tester.load_config([config])

    ubusd.send_event("interface.state", {"state": "ifup", "interface": interface_name})
    start_time = time.perf_counter()
    time_allowance_for_test_secs = 1
    max_seconds_to_wait = (
        config.config.settling_delay_secs + config.config.response_timeout_secs + time_allowance_for_test_secs
    )
    fail_event = ubus_listener.wait_for_event(
        "interface.tester.test_run", {"result": "fail", "interface": interface_name}, max_seconds_to_wait
    )
    seconds_to_fail = fail_event.timestamp - start_time
    assert seconds_to_fail < max_seconds_to_wait,  f"took too long for test to fail ({seconds_to_fail})"
    broken_event = ubus_listener.wait_for_event(
        "interface.tester.operational", {"is_operational": False, "interface": interface_name}, 1
    )
    assert broken_event.timestamp - fail_event.timestamp < 1, \
        "broken event should occur directly after first test run failure"


def test_interface_tester_single_passing_test_one_test_must_pass_passes_test_run(
    interface_tester: InterfaceTester, pytestconfig: Config, ubus_listener: UbusListener, ubusd: Ubus
) -> None:
    ubus_listener.listen()
    interface_tester.start(
        pytestconfig.getoption("config"), pytestconfig.getoption("tests"), pytestconfig.getoption("tasks")
    )
    interface_name = "wan"
    ubus_listener.wait_for_event("interface.tester", {"state": "up"}, 5)
    config = IfaceTesterInterfaceConfig(
        name=interface_name,
        config=IfaceTesterConfig(
            success_condition=SuccessCondition.ONE_TEST_MUST_PASS,
            tests=[IfaceTesterTestConfig(executable="passing_test", label="passing test")]
        ),
    )
    interface_tester.load_config([config])

    ubusd.send_event("interface.state", {"state": "ifup", "interface": interface_name})
    time_allowance_for_test_secs = 1
    max_seconds_to_wait = (
        config.config.settling_delay_secs + time_allowance_for_test_secs
    )

    ubus_listener.wait_for_event(
        "interface.tester.test_run",
        {"result": "pass", "interface": interface_name},
        max_seconds_to_wait,
    )


def test_interface_tester_one_test_must_pass_last_test_passes_passes_test_run(
    interface_tester: InterfaceTester, pytestconfig: Config, ubus_listener: UbusListener, ubusd: Ubus
) -> None:
    ubus_listener.listen()
    interface_tester.start(
        pytestconfig.getoption("config"), pytestconfig.getoption("tests"), pytestconfig.getoption("tasks")
    )
    interface_name = "wan"
    ubus_listener.wait_for_event("interface.tester", {"state": "up"}, 5)
    config = IfaceTesterInterfaceConfig(
        name=interface_name,
        config=IfaceTesterConfig(
            success_condition=SuccessCondition.ONE_TEST_MUST_PASS,
            tests=[
                IfaceTesterTestConfig(executable="failing_test", label="Failing test"),
                IfaceTesterTestConfig(executable="passing_test", label="Passing test"),
            ]
        ),
    )
    interface_tester.load_config([config])

    ubusd.send_event("interface.state", {"state": "ifup", "interface": interface_name})
    time_allowance_for_test_secs = 1
    max_seconds_to_wait = (
        config.config.settling_delay_secs + time_allowance_for_test_secs
    )

    ubus_listener.wait_for_event(
        "interface.tester.test_run",
        {"result": "pass", "interface": interface_name},
        max_seconds_to_wait,
    )


def test_interface_tester_one_test_must_pass_all_tests_fail_fails_test_run(
    interface_tester: InterfaceTester, pytestconfig: Config, ubus_listener: UbusListener, ubusd: Ubus
) -> None:
    ubus_listener.listen()
    interface_tester.start(
        pytestconfig.getoption("config"), pytestconfig.getoption("tests"), pytestconfig.getoption("tasks")
    )
    interface_name = "wan"
    ubus_listener.wait_for_event("interface.tester", {"state": "up"}, 5)
    config = IfaceTesterInterfaceConfig(
        name=interface_name,
        config=IfaceTesterConfig(
            success_condition=SuccessCondition.ONE_TEST_MUST_PASS,
            tests=[
                IfaceTesterTestConfig(executable="failing_test", label="Failing test"),
                IfaceTesterTestConfig(executable="failing_test", label="Failing test"),
            ]
        ),
    )
    interface_tester.load_config([config])
    ubusd.send_event("interface.state", {"state": "ifup", "interface": interface_name})
    time_allowance_for_test_secs = 1
    max_seconds_to_wait = config.config.settling_delay_secs + time_allowance_for_test_secs
    ubus_listener.wait_for_event(
        "interface.tester.test_run",
        {"result": "fail", "interface": interface_name},
        max_seconds_to_wait,
    )
