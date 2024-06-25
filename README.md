# Interface tester

## Summary
Interface tester is an application intended to perform user-defined tests over an interface that confirm the interface 
is operating as intended. 
The user configures a set of tests that are to be executed at regular intervals. This is called a test run.
If the tests indicate that the interface is broken then user-defined recovery tasks are executed. Recovery tasks should
perform a task that might cause the interface to start working again (e.g. they might simply restart the interface, or 
in extreme cases, reboot the device).
In addition to the recovery tasks, the application sends out ubus events that other applications can listen for and 
perform some other task (e.g. send an email).  
The current internal state of the interface tester can be obtained by making a ubus call.  
e.g. 
```console
ubus call interface.tester.interface.wan state
```

## Command line
```console
# interface_tester -?
interface_tester: unrecognized option: ?
Usage: interface_tester [options]
Options:
 -c <path>:        Path to the configuration file
 -s <path>:        Path to the ubus socket
 -S <path>:        Path to the test executable directory
 -r <path>:        Path to the recovery executable directory

```
e.g.
```console
/usr/bin/interface_tester -S /etc/interface_tester/tests -r /etc/interface_tester/recovery -c /tmp/interface_tester.json
```
All of the options are, unsurprisingly, optional.  
If unspecified, the default ubus socket path is used.  
If unspecified, the current working directory is expected to contain the test
executables. This path would normally be specified.
In unspecified, the current working directory is expected to contain the
recovery task executables. This path would normally be specified.  
If a configuration file is not specified, the configuration will need to be
passed to the application using a ubus call  
e.g.
```console
ubus call interface.tester config "<configuration json>"
```

## Configuration
The application expects the configuration to either a JSON configuration file, or
supplied by way of a ubus call.  
e.g.  
```json
{
	"interfaces": {
		"wan": {
			"success_condition": "all_tests_must_pass",
			"settling_delay_secs": 5,
			"passing_interval_secs": 900,
			"failing_interval_secs": 4,
			"pass_threshold": 3,
			"fail_threshold": 4,
			"response_timeout_secs": 16,
			"failing_tests_metrics_increase": 1,
			"tests": [
				{
					"params": {
						"hostname": "8.8.8.8",
						"count": "1"
					},
					"executable": "ping",
					"label": "Ping Google",
					"response_timeout_secs": 5
				},
				{
					"params": {
						"hostname": "1.1.1.1",
						"count": "1"
					},
					"executable": "ping",
					"label": "Ping alt. Google",
					"response_timeout_secs": 5
				}
			],
			"recovery_tasks": [
				{
					"params": {
						"some_param": "some_value",
						"another_param": "another_value"
					},
					"executable": "ping",
					"label": "Ping Google",
					"response_timeout_secs": 5
				}
			]
		}
	}
}
```
The configuration for each interface that the application should test should be
contained within an "interfaces" section. In the example above the file contains
configuration for one interface called "wan". The name of the interface should
match the interface name as used by netifd.

### Parameters
#### success_condition  
##### Description
The number of tests that need to pass for a test run to be considered successful
##### Valid values
    "all_tests_must_pass" - all configured tests must pass  
    "one_test_must_pass" - one of the configured tests must pass
##### Notes
-- When set to "all_tests_must_pass" a test run will stop as soon as any test in the run fails      
-- When set to "one_test_must_pass" a test run will stop as soon as any test in the run passes

#### settling_delay_secs
##### Description
The number of seconds to wait after the interface connects before initiating a 
test run
##### Valid values
    >= 0  
#### passing_interval_secs
##### Description
The number of seconds between test runs when the tests are passing and the
interface is in the 'operational' state
##### Valid Values
    > 0
#### failing_interval_secs
##### Description
The number of seconds between test runs when tests are failing. This interval is
applied even when tests have started passing, but the interface is still in the
'broken' state because the pass threshold hasn't been met. This interval is also
applied when the interface is 'operational', but tests are failing. This value
would typically be smaller than the passing interval so that a 'broken' 
interface can be detected more quickly.
##### Valid values
    > 0
#### pass_threshold
##### Description
The number of consecutive passing test runs that need to occur before the 
interface will transition from the 'broken' state to the 'operational' state
##### Valid values
    > 0
#### fail_threshold
##### Description
The number of consecutive failing test runs that need to occur before the
interface will transition from the 'operational' state to the 'broken' state
##### Valid values
    > 0
#### response_timeout_secs
##### Description
The maximum number of seconds allowed for an individual test to complete. If a
test hasn't completed within this time the test process is killed and the test
counts as a failure
##### Valid values
    > 0
#### failing_tests_metrics_increase
##### Description
The amount to increase the metric of routes associated with this interface by
when the interface transitions to the 'broken' state. The adjustment will remain
until the interface transitions back to the 'operational' state.  
Note that this feature will only work when a version of netifd that supports 
adjustment of metrics is running on the device.
##### Valid values
    >= 0

### test parameters
The tests array should contain a list of json objects, each containing the
following parameters

#### params
##### Description
This object may contain anything that the user wishes to be passed to the test
##### Valid values
This object must be present (an empty object) even if the test needs no extra 
parameters to be supplied. Otherwise, this parameter may contain any valid json

#### executable
##### Description
This is the name of the executable to run to perform the test. The executable 
itself should be located in the test directory that is passed to the 
interface_tester application on the command line

#### label
##### Description
An identifying label for this test (e.g. "Ping Google")
##### Valid values
    Any valid string

#### response_timeout_secs (optional)
##### Description
If present, this value will override the default response timeout in the main
configuration. This allows the user to allow a different amount of time than 
usual if a test takes an unusual amount of time to complete.
##### Valid values
    If present, the value must be >= 0

### recovery_task parameters
The recovery_tasks array should contain a list of json objects, each containing 
the following parameters

#### params
##### Description
This object may contain anything that the user wishes to be passed to the 
recovery task
##### Valid values
This object must be present (an empty object) even if the task needs no extra 
parameters to be supplied. Otherwise, this parameter may contain any valid json

#### executable
##### Description
This is the name of the executable to run to perform the recovery task. The 
executable itself should be located in the task directory that is passed to the 
interface_tester application on the command line

#### label
##### Description
An identifying label for this test (e.g. "Restart interface")
##### Valid values
    Any valid string

#### response_timeout_secs (optional)
##### Description
If present, this value will override the default response timeout in the main
configuration. This allows the user to allow a different amount of time than 
usual if a recovery task takes an unusual amount of time to complete.
##### Valid values
    If present, the value must be >= 0

## UBUS commands

### Feed configuration to the application
```console
ubus call interface.tester config "<configuration json>"
```

### Reload the configuration from the file specified on the command line
```console
ubus call interface.tester config_reload
```

### Show the state of all configured interface testers
```console
ubus call interface.tester state
```

### Show the state of a single interface
```console
ubus call interface.tester.interface.wan state
```
e.g.
```console
# ubus call interface.tester.interface.wan state
{
	"state": {
		"interface": {
			"connected": "yes",
			"state": "connected",
			"settling_delay_timer": {
				"running": false,
				"remaining": -1
			}
		},
		"tester": {
			"test_index": 0,
			"state": "sleeping",
			"operational_state": "operational",
			"metrics_are_adjusted": false,
			"test_response_timer": {
				"running": false,
				"remaining": -1
			},
			"test_interval_timer": {
				"running": true,
				"remaining": 133084
			},
			"recovery_task_timer": {
				"running": false,
				"remaining": -1
			},
			"test_process_running": false,
			"last_test_exit_code": 0,
			"last_test_passed": true,
			"recovery_task_running": false,
			"stats": {
				"test_runs": {
					"consecutive_passes": 1,
					"total_passes_this_connection": 1,
					"total_passes": 1,
					"consecutive_failures": 0,
					"total_failures_this_connection": 0,
					"total_failures": 0
				},
				"tests": {
					"total_passes_this_connection": 2,
					"total_passes": 2,
					"total_failures_this_connection": 0,
					"total_failures": 0
				},
				"recovery": {
					"total_this_connection": 0,
					"total": 0
				}
			}
		}
	},
	"config": {
		"success_condition": "all_tests_must_pass",
		"settling_delay_secs": 5,
		"passing_interval_secs": 900,
		"failing_interval_secs": 4,
		"pass_threshold": 4,
		"fail_threshold": 3,
		"response_timeout_secs": 15,
		"failing_tests_metrics_increase": 1,
		"tests": [
			{
				"executable": "ping",
				"label": "Ping Google",
				"response_timeout_secs": 5,
				"params": {
					"hostname": "8.8.8.8",
					"count": "1"
				}
			},
			{
				"executable": "ping",
				"label": "Ping alt. Google",
				"response_timeout_secs": 5,
				"params": {
					"hostname": "1.1.1.1",
					"count": "1"
				}
			}
		],
		"recovery_tasks": [
			
		]
	}
}
```

## UBUS events
Ubus events are sent out by the application under certain circumstances.

### interface.tester.test_run events
interface.tester.test_run events are sent out at the end of each test run. The
event contains the interface name and the result of the test (either "passed" or "failed").  
e.g.
```console
{ "interface.tester.test_run": {"result":"fail","interface":"wan"} }
{ "interface.tester.test_run": {"result":"pass","interface":"wan"} }
```
### interface.tester.operational events
interface.tester.operational events are sent out whenever the tester switches between
operational and broken.  
e.g.
```console
{ "interface.tester.operational": {"is_operational":false,"interface":"wan"} }
{ "interface.tester.operational": {"is_operational":true,"interface":"wan"} }
```


## Notes
- When the program first starts, the interface will start out in the 'operational'
state. However, if the interface transitions to the 'broken' state and the 
interface disconnects and reconnects (e.g. due to a recovery task), the 
interface will remain in the 'broken' state.
- It is not necessary to provide any recovery tasks. This can be useful in the
case where tests are being created and debugged, and it would be inconvenient 
for recovery tasks to interfere with the state of the interface.
- A test executable is passed the interface name (e.g. "wan"), the name of the
executable and the parameters defined in the JSON configuration file specific to 
the test on the command line, in that order.  
e.g.
```console
/etc/interface/tester/ping wan ping "{\"hostname=\"1.1.1.1\", \"count\"=\"1\"}"
```
- A test should return 0 to indicate success, and non-zero to indicate failure.
- Similar to tests, recovery tasks are passed the interface name (e.g. "wan"), 
the name of the executable and the parameters defined in the JSON configuration 
file specific to the test on the command line, in that order.
- If either a test or a recovery task takes too long to execute (i.e the 
response time is exceeded), the process will be killed. For tests, this counts
as a failed test.
- If an interface disconnects while a test is in progress, the test will be 
killed. No testing is performed when an interface is disconnected.
- If an interface disconnects while a recovery task is in progress, that task
will not be killed. Recovery tasks may disconnect an interface intentionally,
and may not have completed before the interface disconnects.
 
