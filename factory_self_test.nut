// Copyright 2014 hiku labs inc. All rights reserved. Confidential.
// Setup the server to behave when we have the no-wifi condition
server.setsendtimeoutpolicy(RETURN_ON_ERROR, WAIT_TIL_SENT, 30);

local connection_available = false;

//
// I2C addresses and registers
//

// SX1508 or SX1505/2 I2C I/O expanders
// Board is populated with either SX1508, SX1505, or SX1502.
const SX1508ADDR = 0x23;
const SX1505ADDR = 0x21;

// 0x0D is used for misc instead of the 0x10
local sx1508reg = {RegMisc = 0x10,
    RegLowDrive = 0x02,
    RegInterruptSource = 0x0C,
    RegData = 0x08,
    RegDir = 0x07,
    RegPullUp = 0x03,
    RegPullDown = 0x04,
    //RegOpenDrain = 0x05,
    RegInterruptMask = 0x09,
    RegSenseLow = 0x0B};

local sx1505reg = {RegMisc = null,
    RegLowDrive = null,
    RegInterruptSource = 0x08,
    RegData = 0x00,
    RegDir = 0x01,
    RegPullUp = 0x02,
    RegPullDown = 0x03,
    RegInterruptMask = 0x05,
    RegSenseLow = 0x07};

i2cReg <- null;

// ST LIS3DHTR accelerometer
const ACCELADDR  = 0x18;

const ACCEL_CTRL_REG1     = 0x20;
const ACCEL_CTRL_REG2     = 0x21;
const ACCEL_CTRL_REG3     = 0x22;
const ACCEL_CTRL_REG4     = 0x23;
const ACCEL_CTRL_REG5     = 0x24;
const ACCEL_INT1_CFG      = 0x30;
const ACCEL_INT1_SRC      = 0x31;
const ACCEL_INT1_THS      = 0x32;
const ACCEL_INT1_DURATION = 0x33;

//
// Audio: microphone and buzzer
//

const AUDIO_SAMPLE_RATE = 8000; // recording rate in Hz
//
// Amplitude and range values are based on a 12-bit ADC
// with values from 0..4095; 0 -> 0V; 4095 -> VREF=3V 
//
const ADC_MAX = 4095;
// Allowable noise amplitude when recording silence 
// with and without WiFi running
const AUDIO_SILENCE_AMP_WIFI = 128;
const AUDIO_SILENCE_AMP_NO_WIFI = 128;

// A/D converter readings should be at mid point when recording silence,
// i.e. 4096/2 for left-aligned data, allowing +/- 128.
const AUDIO_MID = 2048;
const AUDIO_MID_VARIANCE = 128;

// Minimum and maximum amplitude when recording the buzzer
const AUDIO_BUZZER_AMP_MIN = 280;
const AUDIO_BUZZER_AMP_MAX = 1250;

// Number of min/max values to store for amplitude evaluation
const AUDIO_NUM_VALUES = 20;

// buffers for recording audio
const AUDIO_BUFFER_SIZE = 2000; // size of each audio buffer 
AUDIO_BUF0 <- blob(AUDIO_BUFFER_SIZE);
AUDIO_BUF1 <- blob(AUDIO_BUFFER_SIZE);
AUDIO_BUF2 <- blob(AUDIO_BUFFER_SIZE);
AUDIO_BUF3 <- blob(AUDIO_BUFFER_SIZE);

// test sequence
const AUDIO_TEST_SILENCE      = 0;
const AUDIO_TEST_SILENCE_WIFI = 1;
const AUDIO_TEST_BUZZER       = 2;
const AUDIO_TEST_DONE         = 3;

audioCurrentTest <- AUDIO_TEST_SILENCE;
audioBufCount <- 0;
audioMin <- array(AUDIO_NUM_VALUES, ADC_MAX);
audioMax <- array(AUDIO_NUM_VALUES, 0);

//
// Test message types
//	
	
// Informational message only		
const TEST_INFO = 1;
// Test step succeeded
const TEST_SUCCESS = 2;
// Test step failed, but the test was not essential to provide full
// user-level functionality. For example, an unconnected pin on the Imp that is
// shorted provides an indication of manufacturing problems, but would not impact
// user-level functionality.
const TEST_WARNING = 3;
// Test step failed, continue testing. An essential test failed, end result will be FAIL.
const TEST_ERROR = 4;
// Test step failed, abort test. An essential test failed and does not allow for
// further testing. For example, if the I2C bus is shorted, all other tests would
// fail, and the Imp could be damaged by continuing the test.
const TEST_FATAL = 5;
// Test is complete, test report is final.
const TEST_FINAL = 6;

//
// Pin drive types
//
const DRIVE_TYPE_FLOAT = 0;
const DRIVE_TYPE_PD    = 1;
const DRIVE_TYPE_PU    = 2;
const DRIVE_TYPE_LO    = 3;
const DRIVE_TYPE_HI    = 4;

//
// Component classes to be tested
//
const TEST_CLASS_NONE    = 0;
const TEST_CLASS_DEV_ID  = 1
const TEST_CLASS_LED     = 2;
const TEST_CLASS_IMP_PIN = 3;
const TEST_CLASS_IO_EXP  = 4;
const TEST_CLASS_ACCEL   = 5;
const TEST_CLASS_SCANNER = 6;
const TEST_CLASS_AUDIO   = 7;
const TEST_CLASS_CHARGER = 8;
const TEST_CLASS_BUTTON  = 9;

//
// Pin assignment on the Imp
//
CPU_INT             <- hardware.pin1;
EIMP_AUDIO_IN       <- hardware.pin2; // "EIMP-AUDIO_IN" in schematics
EIMP_AUDIO_OUT      <- hardware.pin5; // "EIMP-AUDIO_OUT" in schematics
RXD_IN_FROM_SCANNER <- hardware.pin7;
SCL_OUT             <- hardware.pin8;
SDA_OUT             <- hardware.pin9;
BATT_VOLT_MEASURE   <- hardware.pinB;
CHARGE_DISABLE_H    <- hardware.pinC;

//
// Pin assignment on the I2C I/O expander
//
const ACCELEROMETER_INT  = 0;
const CHARGE_STATUS_L    = 1;
const BUTTON_L           = 2;
const IO_EXP3_UNUSED     = 3;
const SW_VCC_EN_L        = 4;
const SCAN_TRIGGER_OUT_L = 5;
const SCANNER_RESET_L    = 6;
const CHARGE_PGOOD_L     = 7;

//
// Battery and charger constants
//
// maximal and minimal battery voltages acceptable during assembly/test
//
const BATT_MAX_VOLTAGE      = 4.2;
// Battery voltage needs to be at least above V_LOWV=3.1V of BQ24072
// (3.2V with margin for error ) such that charging in testing is done
// in fast-charge mode.
const BATT_MIN_VOLTAGE      = 3.2;
// Issue a battery warning if below 3.5V as we wouldn't want to ship
// devices with empty batteries.
const BATT_MIN_WARN_VOLTAGE = 3.5;
// Reference voltage VREF, min and max.
const VREF_MIN = 2.9;
const VREF_MAX = 3.1;
// minimal voltage increase when charger is plugged in
const CHARGE_MIN_INCREASE = 0.03;
// ADC resolution is 12 bits; 2^12=4096
// Resistor divider R4/R9 is 40.2k/80.6k
// VREF = 3V
const BATT_ADC_RDIV    = 0.001097724100496; // =  3*(40.2+80.6)/(80.6 * 4096)
// number of ADC samples to average over
const BATT_ADC_SAMPLES  = 20

//
// MT700 scanner
//

const TEST_REFERENCE_BARCODE = "123456789012\r\n";
scannerUart <- hardware.uart57;

// set test_ok to false if any one test fails
test_ok <- true;

gIsConnecting <- false;

// This NV persistence is only good for warm boot
// if we get a cold boot, all of this goes away
// If a cold persistence is required then we need to
// user the server.setpermanent() api
if (!("nv" in getroottable()))
{
    nv <- { 
    	setup_count = 0,
    	disconnect_reason=0, 
    };
}

const CONNECT_RETRY_TIME = 45; // for now 45 seconds retry time

//======================================================================
// Class to handle all the connection management and retry mechanisms
// This will self contain all the wifi connection establishment and retries

class ConnectionManager
{
	_connCb = array(2);
	numCb = 0;
	
	constructor()
	{
	}
	
	function registerCallback(func)
	{
		if( numCb > _connCb.len() )
		{
			_connCb.resize(numCb+2);
		}
		
		_connCb[numCb++] = func;
	}
	
	function init_connections()
	{
 		if( server.isconnected() )
		{
			notifyConnectionStatus(SERVER_CONNECTED);
		}
		else
		{
			server.connect(onConnectedResume.bindenv(this), CONNECT_RETRY_TIME);
		}


    	server.onunexpecteddisconnect(onUnexpectedDisconnect.bindenv(this));
		server.onshutdown(onShutdown.bindenv(this));    	
    	
	}
	
	function notifyConnectionStatus(status)
	{
		local i = 0;
		for( i = 0; i < numCb; i++ )
		{
			_connCb[i](status);
		}
	}
	

	//********************************************************************
	// This is to make sure that we wake up and start trying to connect on the background
	// only time we would get into onConnected is when we have a connection established
	// otherwise its going to beep left and right which is nasty!
	function onConnectedResume(status)
	{
		if( status != SERVER_CONNECTED)
		{
			nv.disconnect_reason = status;
			imp.wakeup(2, tryToConnect.bindenv(this) );
			//hwPiezo.playSound("no-connection");
			connection_available = false;
		}
		else
		{
			connection_available = true;
			notifyConnectionStatus(status);
		}
	}

	function tryToConnect()
	{
    	if (!server.isconnected() && !gIsConnecting) {
    		gIsConnecting = true;
        	server.connect(onConnectedResume.bindenv(this), CONNECT_RETRY_TIME);
        	imp.wakeup(CONNECT_RETRY_TIME+2, tryToConnect.bindenv(this));
    	}
	}

	function onUnexpectedDisconnect(status)
	{
    	nv.disconnect_reason = status;
    	connection_available = false;
    	notifyConnectionStatus(status);
    	if( !gIsConnecting )
    	{
   			imp.wakeup(0.5, tryToConnect.bindenv(this));
   		}
	}
	

	function onShutdown(status)
	{
		agentSend("shutdownRequestReason", status);
		if((status == SHUTDOWN_NEWSQUIRREL) || (status == SHUTDOWN_NEWFIRMWARE))
		{
			hwPiezo.playSound("software-update");
			imp.wakeup(2, function(){
				server.restart();
			});
		}
		else
		{
			server.restart();
		}
	}		
}

//======================================================================
// Handles all audio output
class Piezo
{
    // The hardware pin controlling the piezo 
    pin = null;
    
    page_device = false;
	pageToneIdx=0;
		
    // In Squirrel, if you initialize a member array or table, all
    // instances will point to the same one.  So init in the constructor.
    tonesParamsList = {};

    // State for playing tones asynchronously
    currentToneIdx = 0;
    currentTone = null;

    // Audio generation constants
    static noteB4 = 0.002025; // 494 Hz 
    static noteE5 = 0.001517; // 659 Hz
    static noteE6 = 0.000759; // 1318 Hz
    static dc = 0.5; // 50% duty cycle, maximum power for a piezo
    static longTone = 0.2; // duration in seconds
    static shortTone = 0.15; // duration in seconds
        // rmk experimenting with a new extrashort tone
    static extraShortTone = 0.1; // duration in seconds

    //**********************************************************
    constructor(hwPin)
    {
    	//gInitTime.piezo = hardware.millis();
        pin = hwPin;
        
        // Configure pin
        pin.configure(DIGITAL_OUT);
        pin.write(0); // Turn off piezo by default
		
	disable();

        tonesParamsList = {
            // [[period, duty cycle, duration], ...]
            // 1kHz for 1s
            "one-khz": [[0.001, 0.5, 0.6]],
            "test-fail": [[noteB4, 0.85, 0.5]],
            "test-pass": [[noteE6, 0.85, 0.5]],
            "test-start": [[noteB4, 0.85, longTone], [noteE5, 0.15, shortTone]]
        };

        //gInitTime.piezo = hardware.millis() - gInitTime.piezo;
    }
    
    function disable()
    {
    	pin.configure(DIGITAL_IN_PULLUP);
    }
    
    // utility futimeoutnction to validate that the tone is present
    // and it is not a silent tone
    function validate_tone( tone )
    {
        // Handle invalid tone values
        if (!(tone in tonesParamsList))
        {
            log(format("Error: unknown tone \"%s\"", tone));
            return false;
        }

        // Handle "silent" tones
        if (tonesParamsList[tone].len() == 0)
        {
            return false;
        } 
        return true;   
    }    

    //**********************************************************
    // Play a tone (a set of notes).  Defaults to asynchronous
    // playback, but also supports synchronous via busy waits
    function playSound(tone = "success", async = true) 
    {

	if( !validate_tone( tone ) )
	    return;

        if (async)
        {
            // Play the first note
            local params = tonesParamsList[tone][0];
            pin.configure(PWM_OUT, params[0], params[1]);
            // Play the next note after the specified delay
            currentTone = tone;
            currentToneIdx = 1;
            imp.wakeup(params[2], _continueSound.bindenv(this));
        }
        else 
        {
            // Play synchronously
            foreach (params in tonesParamsList[tone])
            {
                pin.configure(PWM_OUT, params[0], params[1]);
                pin.configure(PWM_OUT, params[0], params[1]);
                imp.sleep(params[2]);
            }
            pin.write(0);
        }
    }
        
    //**********************************************************
    // Continue playing an asynchronous sound. This is the 
    // callback that plays all notes after the first. 
    function _continueSound()
    {
        // Turn off the previous note
        pin.write(0);

        // This happens when playing more than one tone concurrently, 
        // which can happen if you scan again before the first tone
        // finishes.  Long term solution is to create a queue of notes
        // to play. 
        if (currentTone == null)
        {
            log("Error: tried to play null tone");
            return;
        }

        // Play the next note, if any
        if (tonesParamsList[currentTone].len() > currentToneIdx)
        {
            local params = tonesParamsList[currentTone][currentToneIdx];
           // local params1 = tonesParamsList[currentTone][currentToneIdx-1];
            pin.configure(PWM_OUT, params[0], params[1]);

            currentToneIdx++;
            imp.wakeup(params[2], _continueSound.bindenv(this));
        }
        else 
        {
            currentToneIdx = 0;
            currentTone = null;
        }
    }
}


//======================================================================
// Timer that can be canceled and executes a function when expiring
// Now that Electric Imp provides a timer handle each time you set a
// Timer, the CancellableTimer is now constructed to wake up for the set
// timer value instead of doing 0.5 seconds wakeup and checking for elapsed time
// This method significantly reduces the amount of timer interrupts fired
//
// New Method:
// If a timer object is created and enabled then it would use the duration to set the timer
// and retain the handle for it.  When the timer fires, it would call the action function
// if a timer needs to be cancelled, just call the disable function and it would disable the
// timer and set the handle to null.

class CancellableTimer
{
    actionFn = null; // Function to call when timer expires
    _timerHandle = null;
	duration = 0.0;

    //**********************************************************
    // Duration in seconds, and function to execute
    constructor(secs, func)
    {
        duration = secs;
        actionFn = func;
    }

    //**********************************************************
    // Start the timer
	// If the _timerHandle is null then no timer pending for this object
	// just create a timer and set it
    function enable() 
    {
        server.log("Timer enable called");
		
		if(_timerHandle)
		{
		  disable();
		}
		_timerHandle = imp.wakeup(duration,_timerCallback.bindenv(this));
    }

    //**********************************************************
    // Stop the timer
	// If the timer handle is not null then we have a pending timer, just cancel it
	// and set the handle to null
    function disable() 
    {
        server.log("Timer disable called!");
        // if the timerHandle is not null, then the timer is enabled and active
		if(_timerHandle)
		{
		  //just cancel the wakeup timer and set the handle to null
		  imp.cancelwakeup(_timerHandle);
		  _timerHandle = null;
		  server.log("Timer canceled wakeup!");
		}
    }
    
    //**********************************************************
    // Set new time for the timer
	// Expectation is that if an existing timer is pending
	// then the it needs to be disabled prior to setting the duration
	// Pre-Condition: timer is not running
	// Post-Condition: Timer is enabled
    function setDuration(secs) 
    {
		duration = secs;
    }
        

    //**********************************************************
    // Internal function to manage cancelation and expiration
    function _timerCallback()
    {
        server.log("timer fired!");
		actionFn();
		_timerHandle = null;
    }
}


//======================================================================
// Handles any I2C device
class I2cDevice
{
    i2cPort = null;
    i2cAddress = null;
    i2cAddress8B = null;
    test_class = null;

    // HACK set i2cReg in IoExpander class only,
    // figure out how to do this properly by invoking
    // the super class' constructor
    constructor(port, address, test_cls, i2c_reg = null)
    {
        if(port == I2C_12)
        {
            // Configure I2C bus on pins 1 & 2
            hardware.configure(I2C_12);
            i2cPort = hardware.i2c12;
        }
        else if(port == I2C_89)
        {
            // Configure I2C bus on pins 8 & 9
            hardware.configure(I2C_89);
            i2cPort = hardware.i2c89;
        }
        else
        {
            test_log(TEST_CLASS_NONE, TEST_ERROR, "Invalid I2C port");
        }

        // Use the fastest supported clock speed
        i2cPort.configure(CLOCK_SPEED_400_KHZ);

        // Takes a 7-bit I2C address
        i2cAddress = address << 1;
	i2cAddress8B = address;

	test_class = test_cls;
	//i2cReg = i2c_reg;
    }

    // Read a byte address
    function read(register)
    {
        local data = i2cPort.read(i2cAddress, format("%c", register), 1);
        if(data == null)
        {
	    test_log(test_class, TEST_ERROR, format("I2C read, register 0x%x", register));
            // TODO: this should return null, right??? Do better handling.
            // TODO: print the i2c address as part of the error
            return -1;
        }

        return data[0];
    }
    
    // Read a byte address and verify value
    function verify(register, exp_data, log_success=true)
    {
	local read_data = i2cPort.read(i2cAddress, format("%c", register), 1);
	if(read_data[0] != exp_data)
            test_log(test_class, TEST_ERROR, format("I2C verify, register 0x%x, expected 0x%x, read 0x%x", register, exp_data, read_data[0]));
	else 
	    if (log_success)
		test_log(test_class, TEST_SUCCESS, format("I2C verify, register 0x%x, data 0x%x", register, read_data[0]));
    }

    
    // Write a byte address
    function write(register, data)
    {
        if(i2cPort.write(i2cAddress, format("%c%c", register, data)) != 0)
            test_log(test_class, TEST_ERROR, format("I2C write, register 0x%x", register));
    }

    // Write a byte address
    // Read back register contents and verify if exp_data is not null
    function write_verify(register, data, exp_data=null, log_success=true)
    {
	if (exp_data == null)
	    exp_data = data;
        if(i2cPort.write(i2cAddress, format("%c%c", register, data)) != 0)
            test_log(test_class, TEST_ERROR, format("I2C write, register 0x%x", register));
	verify(register, exp_data, log_success);
    }

    function disable()
    {
	// leave pins 8 and 9 floating when not in use in testing
	// should be pulled up by external pull-ups
    	hardware.pin8.configure(DIGITAL_IN);
    	hardware.pin9.configure(DIGITAL_IN);
    }

}

class IoExpander extends I2cDevice
{
    function pin_configure(pin_num, drive_type) {
	if ((pin_num < 0) || (pin_num > 7))
	    test_log(TEST_CLASS_NONE, TEST_FATAL, "Invalid pin number on I2C IO expander.");
	local pin_mask = 1 << pin_num;
	local pin_mask_inv = (~pin_mask) & 0xFF;
	switch (drive_type) 
	{
	    //
	    // NOTE: Different orders of I2C write commands may lead to different
	    //       signal edges when pins are reconfigured.
	    //
	case DRIVE_TYPE_FLOAT: 
	    write(i2cReg.RegPullUp, read(i2cReg.RegPullUp) & pin_mask_inv);
	    write(i2cReg.RegPullDown, read(i2cReg.RegPullDown) & pin_mask_inv);
	    write(i2cReg.RegDir, read(i2cReg.RegDir) | pin_mask); 
	    break;
	case DRIVE_TYPE_PU: 
	    write(i2cReg.RegPullDown, read(i2cReg.RegPullDown) & pin_mask_inv);
	    write(i2cReg.RegPullUp, read(i2cReg.RegPullUp) | pin_mask);
	    write(i2cReg.RegDir, read(i2cReg.RegDir) | pin_mask); 
	    break;
	case DRIVE_TYPE_PD: 
	    write(i2cReg.RegPullUp, read(i2cReg.RegPullUp) & pin_mask_inv);
	    write(i2cReg.RegPullDown, read(i2cReg.RegPullDown) | pin_mask);
	    write(i2cReg.RegDir, read(i2cReg.RegDir) | pin_mask); 
	    break;
	case DRIVE_TYPE_LO: 
	    write(i2cReg.RegData, read(i2cReg.RegData) & pin_mask_inv); 
	    write(i2cReg.RegDir, read(i2cReg.RegDir) & pin_mask_inv); 
	    write(i2cReg.RegPullUp, read(i2cReg.RegPullUp) & pin_mask_inv);
	    write(i2cReg.RegPullDown, read(i2cReg.RegPullDown) & pin_mask_inv);
	    break;
	case DRIVE_TYPE_HI: 
	    write(i2cReg.RegData, read(i2cReg.RegData) | pin_mask); 
	    write(i2cReg.RegDir, read(i2cReg.RegDir) & pin_mask_inv); 
	    write(i2cReg.RegPullUp, read(i2cReg.RegPullUp) & pin_mask_inv);
	    write(i2cReg.RegPullDown, read(i2cReg.RegPullDown) & pin_mask_inv);
	    break;
	default:
	    test_log(TEST_CLASS_NONE, TEST_FATAL, "Invalid signal drive type.");
	}	
    }
    
    // Writes data to pin only. Assumes that pin is already configured as an output.
    function pin_write(pin_num, value) {
	if ((pin_num < 0) || (pin_num > 7) || (value < 0) || (value > 1))
	    test_log(TEST_CLASS_NONE, TEST_FATAL, "Invalid pin number or signal value on I2C IO expander.");
	local read_data = read(i2cReg.RegData);
	local pin_mask = 1 << pin_num;
	local pin_mask_inv = (~pin_mask) & 0xFF;
	if (value == 0) {
	    write(i2cReg.RegData, read(i2cReg.RegData) & pin_mask_inv); 
	} else {
	    write(i2cReg.RegData, read(i2cReg.RegData) & pin_mask_inv); 	    
	}
	if (read_data == -1)
	    return read_data;
	else
	    return ((read_data >> pin_num) & 0x01)
    }

    function pin_read(pin_num) {
	if ((pin_num < 0) || (pin_num > 7))
	    test_log(TEST_CLASS_NONE, TEST_FATAL, "Invalid pin number on I2C IO expander.");
	local read_data = read(i2cReg.RegData);
	if (read_data == -1)
	    return read_data;
	else
	    return ((read_data >> pin_num) & 0x01)
    }

    // Configure a pin, drive it as indicated by drive_type, read back the actual value, 
    // quickly re-configure as input (to avoid possible damage to the pin), compare
    // actual vs. expected value.
    function pin_fast_probe(pin_num, expect, drive_type, name, failure_mode=TEST_ERROR) {
	pin_configure(pin_num, drive_type);
	local actual = pin_read(pin_num);
	// Quickly turn off the pin driver after reading as driving the pin may
	// create a short in case of a PCB failure.
	pin_configure(pin_num, DRIVE_TYPE_FLOAT);
	if (actual != expect)
	    test_log(TEST_CLASS_IO_EXP, failure_mode, format("%s, expected %d, actual %d.", name, expect, actual));
	else
	    test_log(TEST_CLASS_IO_EXP, TEST_SUCCESS, format("%s. Value %d.", name, expect));
    }
}

function test_log(test_class, test_result, log_string) {

    // Create one file with sequence of test messages
    // and individual files for each category.
    // Allows for comparing files for "ERROR" and "SUCCESS"
    // against a known good to determine test PASS or FAIL.
    local msg_str;
    local log_str;
    switch (test_result) 
    {
    case TEST_INFO: msg_str = "INFO:"; break;
    case TEST_SUCCESS: msg_str = "SUCCESS:"; break;
    case TEST_WARNING: msg_str = "WARNING:"; break;
    case TEST_ERROR: 
	msg_str = "ERROR:";
	test_ok = false;
	break;
    case TEST_FATAL: 
	msg_str = "FATAL:"; 
	test_ok = false;
	break;
    case TEST_FINAL:
	if (test_ok) {
	    msg_str = "PASS:";
	    log_str = "All tests passed.";
	    hwPiezo.playSound("test-pass", false);
	}
	else {
	    msg_str = "FAIL:";
	    log_str = "Test(s) failed.";
	    hwPiezo.playSound("test-fail", false);
	    // report what failed
	}
	break;
    default:
	test_result = TEST_FATAL;
	msg_str = "FATAL:";
	log_string = "Invalid test_result value";
    }
    // HACK
    // change test report format
    // create a test report 
    server.log(format("%d %s %s", test_class, msg_str, log_string));

    if (test_result == TEST_FATAL)
	factoryTester.testFinish();
    else 
	return true;
}

class FactoryTester {
    // sleep time between I2C reconfiguration attempts
    i2c_sleep_time = 0.2;
    test_start_time = 0;
    i2cIOExp = null;
    
    constructor()
    {
    }

    function pin_validate(pin, expect, name, failure_mode=TEST_ERROR) {
	local actual = pin.read();
	if (actual != expect)
	    test_log(TEST_CLASS_IMP_PIN, failure_mode, format("Read pin %s, expected %d, actual %d.", name, expect, actual));
	else
	    test_log(TEST_CLASS_IMP_PIN, TEST_SUCCESS, format("Read pin %s as %d.", name, expect));
    }

    // Configure a pin, drive it as indicated by drive_type, read back the actual value, 
    // quickly re-configure as input (to avoid possible damage to the pin), compare
    // actual vs. expected value.
    function pin_fast_probe(pin, expect, drive_type, name, failure_mode=TEST_ERROR) {
	switch (drive_type) 
	{
	case DRIVE_TYPE_FLOAT: pin.configure(DIGITAL_IN); break;
	case DRIVE_TYPE_PU: pin.configure(DIGITAL_IN_PULLUP); break;
	case DRIVE_TYPE_PD: pin.configure(DIGITAL_IN_PULLDOWN); break;
	case DRIVE_TYPE_LO: pin.configure(DIGITAL_OUT); pin.write(0); break;
	case DRIVE_TYPE_HI: pin.configure(DIGITAL_OUT); pin.write(1); break;
	default:
	    test_log(TEST_CLASS_NONE, TEST_FATAL, "Invalid signal drive type.");
	}
	local actual = pin.read();
	// Quickly turn off the pin driver after reading as driving the pin may
	// create a short in case of a PCB failure.
	pin.configure(DIGITAL_IN);
	if (actual != expect)
	    test_log(TEST_CLASS_IMP_PIN, failure_mode, format("%s, expected %d, actual %d.", name, expect, actual));
	else
	    test_log(TEST_CLASS_IMP_PIN, TEST_SUCCESS, format("%s. Value %d.", name, expect));
    }

    function impPinTest() {
	test_log(TEST_CLASS_IMP_PIN, TEST_INFO, "**** IMP PIN TESTS STARTING ****");

	// currently unused pins on the Imp
	local pin6 = hardware.pin6;
	local pinA = hardware.pinA;
	local pinD = hardware.pinD;
	local pinE = hardware.pinE;

	// Drive I2C SCL and SDA pins low and check outputs
	// If they cannot be driven low, the I2C is defective and no further
	// tests are possible.
	pin_fast_probe(SCL_OUT, 0, DRIVE_TYPE_LO, "Testing I2C SCL_OUT for short to VCC", TEST_FATAL);
	pin_fast_probe(SDA_OUT, 0, DRIVE_TYPE_LO, "Testing I2C SDA_OUT for short to VCC", TEST_FATAL);
	// Test external PU resistors on I2C SCL and SDA. If they are not present, the I2C is defective
	// and no further tests are possible.
	pin_fast_probe(SCL_OUT, 1, DRIVE_TYPE_FLOAT, "Testing I2C SCL_OUT for presence of PU resistor", TEST_FATAL);
	pin_fast_probe(SDA_OUT, 1, DRIVE_TYPE_FLOAT, "Testing I2C SDA_OUT for presence of PU resistor", TEST_FATAL);

	// Drive buzzer pin EIMP-AUDIO_OUT high and check output
	pin_fast_probe(EIMP_AUDIO_OUT, 1, DRIVE_TYPE_HI, "Testing EIMP-AUDIO_OUT for short to GND");

	// Configure all digital pins on the Imp that have external drivers to floating input.
	CPU_INT.configure(DIGITAL_IN);
	RXD_IN_FROM_SCANNER.configure(DIGITAL_IN);

	// Configure audio and battery voltage inputs as analog
	EIMP_AUDIO_IN.configure(ANALOG_IN);
	BATT_VOLT_MEASURE.configure(ANALOG_IN);

	// Check pin values
	// CPU_INT can only be checked once I2C IO expander has been configured
	// EIMP_AUDIO_IN can only be checked with analog signal from audio amplifier
	pin_fast_probe(EIMP_AUDIO_OUT, 0, DRIVE_TYPE_FLOAT, "Testing EIMP-AUDIO_OUT for presence of PD resistor");
	pin_fast_probe(pin6, 0, DRIVE_TYPE_PD, "Testing open pin6 for floating with PD resistor", TEST_WARNING);
	pin_fast_probe(pin6, 1, DRIVE_TYPE_PU, "Testing open pin6 for floating with PU resistor", TEST_WARNING);
	// RXD_IN_FROM_SCANNER can only be checked when driven by scanner serial output
	pin_fast_probe(pinA, 0, DRIVE_TYPE_PD, "Testing open pinA for floating with PD resistor", TEST_WARNING);
	pin_fast_probe(pinA, 1, DRIVE_TYPE_PU, "Testing open pinA for floating with PU resistor", TEST_WARNING);
	pin_fast_probe(CHARGE_DISABLE_H, 0, DRIVE_TYPE_PD, "Testing CHARGE_DISABLE_H for floating with PD resistor");
	pin_fast_probe(CHARGE_DISABLE_H, 1, DRIVE_TYPE_PU, "Testing CHARGE_DISABLE_H for floating with PU resistor");
	pin_fast_probe(pinD, 0, DRIVE_TYPE_PD, "Testing open pinD for floating with PD resistor", TEST_WARNING);
	pin_fast_probe(pinD, 1, DRIVE_TYPE_PU, "Testing open pinD for floating with PU resistor", TEST_WARNING);
	pin_fast_probe(pinE, 0, DRIVE_TYPE_PD, "Testing open pinE for floating with PD resistor", TEST_WARNING);
	pin_fast_probe(pinE, 1, DRIVE_TYPE_PU, "Testing open pinE for floating with PU resistor", TEST_WARNING);
	
	// Test neighboring pin pairs for shorts by using a pull-up/pull-down on one
	// and a hard GND/VCC on the other. Check for pin with pull-up/pull-down
	// to not be impacted by neighboring pin.

	test_log(TEST_CLASS_IMP_PIN, TEST_INFO, "**** IMP PIN TESTS DONE ****");
	ioExpanderTest();
    }

    function ledTest() {
	// HACK
	// Automated LED test TBD
	// Tri-color LED is too far from the photo sensor to sense the
	// emitted light for automated testing.
	test_log(TEST_CLASS_LED, TEST_INFO, "**** LED TEST STARTING ****");
	local lightarray = array(0);
	for (local i=0; i<50; i++) {
	    lightarray.push(hardware.lightlevel());
	    imp.sleep(0.2);
	}
	test_log(TEST_CLASS_LED, TEST_INFO, "**** LET TEST DONE ****");
	ioExpanderTest();
    }

    function ioExpanderTest() {
	test_log(TEST_CLASS_IO_EXP, TEST_INFO, "**** I/O EXPANDER TESTS STARTING ****");

	local ioexpaddr = SX1508ADDR;

	// Test for presence of either SX1508 or SX1505/2 IO expander
	hardware.configure(I2C_89);
	imp.sleep(i2c_sleep_time);
	hardware.i2c89.configure(CLOCK_SPEED_400_KHZ); // use fastest clock rate
	// Probe SX1508
	local i2cAddress = SX1508ADDR << 1;	
	local data = hardware.i2c89.read(i2cAddress, format("%c", 0x08), 1);
	if (data == null) { 
	    test_log(TEST_CLASS_IO_EXP, TEST_INFO, format("SX1508 IO expander not found, I2C error code %d. Trying SX1505/2.", hardware.i2c89.readerror()));
	    hardware.i2c89.disable();
	    // Probe SX1505/2
	    imp.sleep(i2c_sleep_time);
	    i2cAddress = SX1505ADDR << 1;
	    hardware.configure(I2C_89);
	    imp.sleep(i2c_sleep_time);
	    hardware.i2c89.configure(CLOCK_SPEED_400_KHZ); // use fastest clock rate			
	    data = hardware.i2c89.read(i2cAddress, format("%c", 0x00), 1);
	    if(data == null) {
		test_log(TEST_CLASS_IO_EXP, TEST_FATAL, format("SX1505/2 IO expander not found (in addition to SX1508), I2C error code %d", hardware.i2c89.readerror()));
	    }
	    else {
		test_log(TEST_CLASS_IO_EXP, TEST_SUCCESS, "Found SX1505/2 IO expander");
		ioexpaddr = SX1505ADDR;
	    }}
	else
	    {
	    // We detected SX1508
	    test_log(TEST_CLASS_IO_EXP, TEST_SUCCESS, "Found SX1508 IO expander");
	    ioexpaddr = SX1508ADDR;
	}
	hardware.i2c89.disable();

	// at this stage if the ioexpaddr is not chosen, then default to SX1508ADDR
	i2cReg = (ioexpaddr == SX1505ADDR) ? sx1505reg:sx1508reg;
	i2cIOExp = IoExpander(I2C_89, ioexpaddr, TEST_CLASS_IO_EXP, i2cReg);

	// Set SX1508/5/2 to default configuration
	// clear all interrupts
	i2cIOExp.write_verify(i2cReg.RegInterruptMask, 0xFF);
	i2cIOExp.write(i2cReg.RegInterruptSource, 0xFF);
	i2cIOExp.write_verify(i2cReg.RegSenseLow, 0x00);
	i2cIOExp.write_verify(i2cReg.RegDir, 0xFF);
	i2cIOExp.write(i2cReg.RegData, 0xFF);
	i2cIOExp.write_verify(i2cReg.RegPullUp, 0x00);
	i2cIOExp.write_verify(i2cReg.RegPullDown, 0x00);

	// Imp CPU_INT should be low with default config of SX1508/5/2,
	// i.e. no interrupt sources and all interrupts cleared
	pin_validate(CPU_INT, 0, "CPU_INT");

	// CHARGE_STATUS_L, CHARGE_PGOOD_L, SW_VCC_EN_OUT_L should be pulled high through
	// external pull-ups when not in use
	i2cIOExp.pin_fast_probe(CHARGE_STATUS_L, 1, DRIVE_TYPE_FLOAT, "Testing CHARGE_STATUS_L for presence of PU resistor");
	i2cIOExp.pin_fast_probe(SW_VCC_EN_L, 1, DRIVE_TYPE_FLOAT, "Testing SW_VCC_EN_L/SW_VCC_EN_OUT_L for presence of PU resistor");
	i2cIOExp.pin_fast_probe(CHARGE_PGOOD_L, 1, DRIVE_TYPE_FLOAT, "Testing CHARGE_PGOOD_L for presence of PU resistor");

	// CHARGE_STATUS_L and CHARGE_PGOOD_L are open-drain on the BQ24072; should be 
	// able to pull them low from the I/O expander    
	i2cIOExp.pin_fast_probe(CHARGE_STATUS_L, 0, DRIVE_TYPE_LO, "Testing CHARGE_STATUS_L for short to VCC");
	i2cIOExp.pin_fast_probe(CHARGE_PGOOD_L, 0, DRIVE_TYPE_LO, "Testing CHARGE_PGOOD_L for short to VCC");

	// BUTTON_L floats when the button is not pressed; should be able to pull it high
	// or low with a pull-up/pull-down
	i2cIOExp.pin_fast_probe(BUTTON_L, 1, DRIVE_TYPE_PU, "Testing BUTTON_L if it can be pulled up");
	i2cIOExp.pin_fast_probe(BUTTON_L, 0, DRIVE_TYPE_PD, "Testing BUTTON_L if it can be pulled down");
	// SCANNER_RESET_L floats; should be able to pull it low with a pull-down
	i2cIOExp.pin_fast_probe(SCANNER_RESET_L, 0, DRIVE_TYPE_PD, "Testing SCANNER_RESET_L if it can be pulled down");
	// should be able to pull it high with a pull-up when the scanner is powered
	i2cIOExp.pin_configure(SW_VCC_EN_L, DRIVE_TYPE_LO);
	i2cIOExp.pin_fast_probe(SCANNER_RESET_L, 1, DRIVE_TYPE_PU, "Testing SCANNER_RESET_L if it can be pulled up");
	i2cIOExp.pin_configure(SW_VCC_EN_L, DRIVE_TYPE_FLOAT);

	test_log(TEST_CLASS_IO_EXP, TEST_INFO, "**** I/O EXPANDER TESTS DONE ****");
	accelerometerTest();
    }

    function accelerometerTest() {
	test_log(TEST_CLASS_ACCEL, TEST_INFO, "**** ACCELEROMETER TESTS STARTING ****");

	// create an I2C device for the LIS3DH accelerometer
	local i2cAccel = I2cDevice(I2C_89, ACCELADDR, TEST_CLASS_ACCEL);

	// test presence of the accelerometer by reading the WHO_AM_I register
	i2cAccel.verify(0x0F, 0x33);

	// setup SX1508/5/2 to accept an interrupt from the accelometer on 
	// both signal edges
	i2cIOExp.write(i2cReg.RegInterruptMask, 0xFE);
	i2cIOExp.write(i2cReg.RegSenseLow, 0x03);

	// Configure accelerometer and enable interrupt
	i2cAccel.write(ACCEL_CTRL_REG1, 0x97); // CTRL_REG1: 1.25kHz (for testing), normal mode, all 3 axes enabled
	i2cAccel.write(ACCEL_CTRL_REG2, 0x00); // CTRL_REG2: Bypass high pass filter
	i2cAccel.write(ACCEL_CTRL_REG3, 0x00); // CTRL_REG3: Disable interrupts
	i2cAccel.write(ACCEL_CTRL_REG4, 0x00); // CTRL_REG4: Default related control settings
	i2cAccel.write(ACCEL_CTRL_REG5, 0x08); // CTRL_REG5: Interrupt latched
	i2cAccel.write(ACCEL_INT1_THS, 0x08);  // INT1_THS: Threshold
	i2cAccel.write(ACCEL_INT1_DURATION, 0x1); // INT1_DURATION: any duration
	i2cAccel.write(ACCEL_INT1_CFG, 0x00); // INT1_CFG: Disable interrupts for now

	// clear the interrupt registers in the accelerometer and in the SX1508/5/2
	i2cAccel.read(ACCEL_INT1_SRC); 
	i2cIOExp.write(i2cReg.RegInterruptSource, 0xFF);
	// verify that interrupt signal CPU_INT at Imp is still at 0
	pin_validate(CPU_INT, 0, "CPU_INT");

	// enable motion interrupt
	i2cAccel.write(ACCEL_CTRL_REG3, 0x40);
	i2cAccel.write(ACCEL_INT1_CFG, 0x3F);
	// enable self test mode to simulate motion; wait for motion to be registered
	i2cAccel.write(ACCEL_CTRL_REG4, 0x02); // CTRL_REG4: Enable self test
	imp.sleep(0.05);

	// verify that the accelerometer triggered the interrupt on signal CPU_INT on the Imp,
	// in register RegInterruptSource on the SX1508/5/2, and in register INT1_SRC on accelerometer
	pin_validate(CPU_INT, 1, "CPU_INT");
	i2cIOExp.verify(i2cReg.RegInterruptSource, 0x01);
	i2cAccel.verify(ACCEL_INT1_SRC, 0x6A); 

	// disable self test and interrupts, clear interrupt bit
	i2cAccel.write(ACCEL_CTRL_REG4, 0x00); 
	i2cAccel.write(ACCEL_CTRL_REG3, 0x00); 
	i2cAccel.write(ACCEL_INT1_CFG, 0x00);
	i2cAccel.read(ACCEL_INT1_SRC); 

	test_log(TEST_CLASS_ACCEL, TEST_INFO, "**** ACCELEROMETER TESTS DONE ****");
	scannerTest();
    }

    function scannerTest() {
	test_log(TEST_CLASS_SCANNER, TEST_INFO, "**** SCANNER TESTS STARTING ****");

	// turn on power to the scanner and reset it
	local regData = i2cIOExp.read(i2cReg.RegData);
	i2cIOExp.write(i2cReg.RegDir, i2cIOExp.read(i2cReg.RegDir) & 0x8F);
	regData = (regData & 0x8F) | 0x20;
	// set SW_VCC_EN_OUT_L (0x10) low to turn on power to scanner
	// set SCANNER_TRIGGER_OUT_L (0x20) high (scanner needs a falling edge to trigger scanning)
	// set SCANNER_RESET_L (0x40) low to reset scanner
	i2cIOExp.write(i2cReg.RegData, regData);
	// set SCANNER_RESET_L (0x40) high to take the scanner out of reset
	regData = regData | 0x40;
	i2cIOExp.write(i2cReg.RegData, regData);
	// configure the UART
	//
	// HACK 
	// uart callback doesn't seem to work
	scannerUart.configure(38400, 8, PARITY_NONE, 1, NO_CTSRTS | NO_TX); //, function() {server.log(hardware.uart57.readstring());});
	// set SCANNER_TRIGGER_OUT_L (0x20) low to turn the scanner on
	regData = regData & 0xDF;
	i2cIOExp.write(i2cReg.RegData, regData);

	local uart_flags = scannerUart.flags();
	local scanWaitCount = 0;
	// try scanning for up to 2s
	while (((uart_flags & READ_READY)==0) && (scanWaitCount < 40)) {
	    scanWaitCount = scanWaitCount + 1;
	    imp.sleep(0.05);
	    uart_flags = scannerUart.flags();
	}
	if (uart_flags & NOISE_ERROR)
	    test_log(TEST_CLASS_SCANNER, TEST_ERROR, "NOISE_ERROR bit set on UART");
	if (uart_flags & FRAME_ERROR)
	    test_log(TEST_CLASS_SCANNER, TEST_ERROR, "FRAME_ERROR bit set on UART");
	if (uart_flags & PARITY_ERROR)
	    test_log(TEST_CLASS_SCANNER, TEST_ERROR, "PARITY_ERROR bit set on UART");
	if (uart_flags & OVERRUN_ERROR)
	    test_log(TEST_CLASS_SCANNER, TEST_ERROR, "OVERRUN_ERROR bit set on UART");
	if (uart_flags & LINE_IDLE)
	    test_log(TEST_CLASS_SCANNER, TEST_INFO, "LINE_IDLE bit set on UART");

	// after 20ms sleep (from the while loop) the full string should be available at
	// both 9600 and 38400 UART bit rates
	imp.sleep(0.02);	
	local scan_string = scannerUart.readstring();

	// turn UART and scanner off, set SW_VCC_EN_OUT_L, SCANNER_TRIGGER_OUT_L, SCANNER_RESET_L to inputs
	scannerUart.disable();
	i2cIOExp.write(i2cReg.RegDir, i2cIOExp.read(i2cReg.RegDir) | 0x70);

	if (scan_string == TEST_REFERENCE_BARCODE) 
	    test_log(TEST_CLASS_SCANNER, TEST_SUCCESS, format("Scanned %s", scan_string));
	else
	    test_log(TEST_CLASS_SCANNER, TEST_ERROR, format("Scanned %s, expected %s", scan_string, TEST_REFERENCE_BARCODE));
	
	test_log(TEST_CLASS_SCANNER, TEST_INFO, "**** SCANNER TESTS DONE ****");

	audioTest();
    }

    function audioTest() {
	i2cIOExp.pin_configure(SW_VCC_EN_L, DRIVE_TYPE_LO);
	test_log(TEST_CLASS_AUDIO, TEST_INFO, "**** AUDIO TESTS STARTING ****");
	// Flush all data to the server to not introduce WiFi
	// noise into audio recording.
	server.flush(1);
	hardware.sampler.reset();
	// record uncompressed 12-bit samples (vs. 8-bit a-law) to simplify
	// data interpretation
	hardware.sampler.configure(EIMP_AUDIO_IN, AUDIO_SAMPLE_RATE, 
            [AUDIO_BUF0, AUDIO_BUF1, AUDIO_BUF2, AUDIO_BUF3], 
            audioCallback);
	audioCurrentTest = AUDIO_TEST_SILENCE;
	audioBufCount = 0;
	audioMin = array(AUDIO_NUM_VALUES, ADC_MAX);
	audioMax = array(AUDIO_NUM_VALUES, 0);
	hardware.sampler.start();
	// audioCallback will be called as buffers fill up 
    }   

    function audioCallback(buffer, length) {
	function comp_vals_up(a, b) {
	    if (a > b) return 1;
	    if (a < b) return -1;
	    return 0;
	}

	function comp_vals_down(a, b) {
	    if (a > b) return -1;
	    if (a < b) return 1;
	    return 0;
	}

	if (length <= 0) {
	    test_log(TEST_CLASS_AUDIO, TEST_ERROR, "Audio sampler buffer overrun.");
	    audioCurrentTest = AUDIO_TEST_DONE;
	}
	else {
	    // HACK better way to generate WiFi traffic?
	    if (audioCurrentTest == AUDIO_TEST_SILENCE_WIFI)
		server.log(buffer);
	    audioBufCount++;
	    if (audioBufCount == 4) 
		hardware.sampler.stop();
	    // Average over AUDIO_NUM_VALUES min and max values
	    // to determine signal amplitude; avoids pass/fail based on outliers.
	    local audio_val;
	    for (local i=0; i<length/2; i++) {
		audio_val = (buffer[2*i+1] << 4) + (buffer[2*i] >> 4);
		if (audio_val < audioMin.top()) {
		    audioMin.pop();
		    audioMin.push(audio_val);
		    audioMin.sort(comp_vals_up);
		} else if (audio_val > audioMax.top()) {
		    audioMax.pop();
		    audioMax.push(audio_val);
		    audioMax.sort(comp_vals_down);
		}
	    }
	    if (audioBufCount == 4) {
		hardware.sampler.reset();
		audioMin = audioMin.reduce(function(prev_val, cur_val){return (prev_val + cur_val)})/audioMin.len();
		audioMax = audioMax.reduce(function(prev_val, cur_val){return (prev_val + cur_val)})/audioMax.len();
		switch (audioCurrentTest) {
		case AUDIO_TEST_SILENCE:
		    if (audioMin > (AUDIO_MID-AUDIO_MID_VARIANCE))
			test_log(TEST_CLASS_AUDIO, TEST_SUCCESS, format("Audio reading min %d.", audioMin));
		    else
			test_log(TEST_CLASS_AUDIO, TEST_ERROR, format("Audio reading min %d, required >%d.", audioMin, AUDIO_MID-AUDIO_MID_VARIANCE));
		    if (audioMax < (AUDIO_MID+AUDIO_MID_VARIANCE))
			test_log(TEST_CLASS_AUDIO, TEST_SUCCESS, format("Audio reading max %d.", audioMax));
		    else
			test_log(TEST_CLASS_AUDIO, TEST_ERROR, format("Audio reading max %d, required <%d.", audioMax, AUDIO_MID+AUDIO_MID_VARIANCE));
		    if ((audioMax-audioMin) < AUDIO_SILENCE_AMP_NO_WIFI)
			test_log(TEST_CLASS_AUDIO, TEST_SUCCESS, format("Audio amplitude recording silence %d (without WiFi).", audioMax-audioMin));
		    else
			test_log(TEST_CLASS_AUDIO, TEST_ERROR, format("Audio amplitude recording silence %d (without WiFi), required <%d.", audioMax-audioMin, AUDIO_SILENCE_AMP_NO_WIFI));
		    audioCurrentTest = AUDIO_TEST_SILENCE_WIFI;
		    break;
		case AUDIO_TEST_SILENCE_WIFI:
		    if ((audioMax-audioMin) < AUDIO_SILENCE_AMP_WIFI)
			test_log(TEST_CLASS_AUDIO, TEST_SUCCESS, format("Audio amplitude recording silence %d (with WiFi).", audioMax-audioMin));
		    else
			test_log(TEST_CLASS_AUDIO, TEST_ERROR, format("Audio amplitude recording silence %d (with WiFi), required <%d.", audioMax-audioMin, AUDIO_SILENCE_AMP_WIFI));
		    audioCurrentTest = AUDIO_TEST_BUZZER;
		    hwPiezo.playSound("one-khz");
		    break;
		case AUDIO_TEST_BUZZER:
		    if (((audioMax-audioMin) > AUDIO_BUZZER_AMP_MIN) && ((audioMax-audioMin) < AUDIO_BUZZER_AMP_MAX))
			test_log(TEST_CLASS_AUDIO, TEST_SUCCESS, format("Audio amplitude recording buzzer %d.", audioMax-audioMin));
		    if ((audioMax-audioMin) <= AUDIO_BUZZER_AMP_MIN)
			test_log(TEST_CLASS_AUDIO, TEST_ERROR, format("Audio amplitude recording buzzer %d, required >%d.", audioMax-audioMin, AUDIO_BUZZER_AMP_MIN));
		    if ((audioMax-audioMin) >= AUDIO_BUZZER_AMP_MAX)
			test_log(TEST_CLASS_AUDIO, TEST_ERROR, format("Audio amplitude recording buzzer %d, required <%d.", audioMax-audioMin, AUDIO_BUZZER_AMP_MAX));
		    audioCurrentTest = AUDIO_TEST_DONE;
		    break;
		case AUDIO_TEST_DONE:
		    break;
		default:
		    test_log(TEST_CLASS_AUDIO, TEST_ERROR, "Invalid audio test.");
		    audioCurrentTest = AUDIO_TEST_DONE;
		    break;
		}
		if (audioCurrentTest != AUDIO_TEST_DONE) {
		    // Flush all data to the server to not introduce WiFi
		    // noise into the following recording.
		    server.flush(1);
		    // record uncompressed 12-bit samples (vs. 8-bit a-law) to simplify
		    // data interpretation
		    hardware.sampler.configure(EIMP_AUDIO_IN, AUDIO_SAMPLE_RATE, 
			[AUDIO_BUF0, AUDIO_BUF1, AUDIO_BUF2, AUDIO_BUF3], 
			factoryTester.audioCallback);
		    audioBufCount = 0;
		    audioMin = array(AUDIO_NUM_VALUES, ADC_MAX);
		    audioMax = array(AUDIO_NUM_VALUES, 0);
		    hardware.sampler.start();		    
		}
	    }
	}
	if (audioCurrentTest == AUDIO_TEST_DONE) {
	    hardware.sampler.stop();
	    hardware.sampler.reset();
	    test_log(TEST_CLASS_AUDIO, TEST_INFO, "**** AUDIO TESTS DONE ****");
	    factoryTester.chargerTest();
	}
    }
 
    function chargerTest() {
	// turn off microphone/scanner after audio test
	i2cIOExp.pin_configure(SW_VCC_EN_L, DRIVE_TYPE_FLOAT);
	
	test_log(TEST_CLASS_CHARGER, TEST_INFO, "**** CHARGER TESTS STARTING ****");

	local vref_voltage = hardware.voltage();
	if ((vref_voltage > VREF_MIN) && (vref_voltage < VREF_MAX))
	    test_log(TEST_CLASS_CHARGER, TEST_SUCCESS, format("Reference voltage VREF %fV.", vref_voltage));
	else
	    test_log(TEST_CLASS_CHARGER, TEST_ERROR, format("Reference voltage VREF %fV out of range, required %fV<VREF<%fV.", vref_voltage, VREF_MIN, VREF_MAX));
	    
	local bat_acc = 0;
	for (local i = 0; i < BATT_ADC_SAMPLES; i++)
    	    bat_acc += (BATT_VOLT_MEASURE.read() >> 4) & 0xFFF;

	local batt_voltage = (bat_acc/BATT_ADC_SAMPLES) * BATT_ADC_RDIV;

	// check battery voltage to be in allowable range
	if (batt_voltage > BATT_MIN_VOLTAGE) {
	    if (batt_voltage < BATT_MAX_VOLTAGE)
		test_log(TEST_CLASS_CHARGER, TEST_SUCCESS, format("Battery voltage %fV.", batt_voltage));
	    else
		test_log(TEST_CLASS_CHARGER, TEST_ERROR, format("Battery voltage %fV higher than allowed %fV.", batt_voltage, BATT_MAX_VOLTAGE));
	} else 
	    test_log(TEST_CLASS_CHARGER, TEST_ERROR, format("Battery voltage %fV lower than allowed %fV.", batt_voltage, BATT_MIN_VOLTAGE));

	// check battery voltage to be in desirable range for shipment to customers
	if (batt_voltage < BATT_MIN_WARN_VOLTAGE)
	    test_log(TEST_CLASS_CHARGER, TEST_WARNING, format("Battery voltage %fV below desired %fV.", batt_voltage, BATT_MIN_WARN_VOLTAGE));

	server.log("waiting for charger");

	local charge_pgood = i2cIOExp.pin_read(CHARGE_PGOOD_L);
	local charge_status = i2cIOExp.pin_read(CHARGE_STATUS_L);
	local chargeWaitCount = 0;
	//
	// HACK
	//
	// Full test suite would have to check for CHARGE_PGOOD_L flashing at 2Hz
	// for safety timer expiration. See http://www.ti.com/lit/ds/symlink/bq24072t.pdf page 23.
	while ((charge_pgood || charge_status) && (chargeWaitCount < 100)) {
	    chargeWaitCount += 1;
	    imp.sleep(0.1);
	    charge_pgood = i2cIOExp.pin_read(CHARGE_PGOOD_L);
	    charge_status = i2cIOExp.pin_read(CHARGE_STATUS_L);
	}

	if (charge_status)
	    test_log(TEST_CLASS_CHARGER, TEST_ERROR, "CHARGE_STATUS_L not low when USB charging.");
	if (charge_pgood)
	    test_log(TEST_CLASS_CHARGER, TEST_ERROR, "CHARGE_PGOOD_L not low when USB charging.");

	// wait for the battery voltage to stabilize with charger switched on
	imp.sleep(0.1);

	bat_acc = 0;
	for (local i = 0; i < BATT_ADC_SAMPLES; i++)
    	    bat_acc += (BATT_VOLT_MEASURE.read() >> 4) & 0xFFF;

	local charge_voltage = (bat_acc/BATT_ADC_SAMPLES) * BATT_ADC_RDIV;
	local volt_diff = charge_voltage - batt_voltage;

	if (volt_diff > CHARGE_MIN_INCREASE)
	    test_log(TEST_CLASS_CHARGER, TEST_SUCCESS, format("Battery voltage when charging %fV greater than before charging.", volt_diff));
	else
	    test_log(TEST_CLASS_CHARGER, TEST_ERROR, format("Battery voltage difference %fV to before charging, requiring greater %fV.", volt_diff, CHARGE_MIN_INCREASE));
	
	test_log(TEST_CLASS_CHARGER, TEST_INFO, "**** CHARGER TESTS DONE ****");
	buttonTest();
    }

    function buttonTest() {
	test_log(TEST_CLASS_BUTTON, TEST_INFO, "**** BUTTON TEST STARTING ****");

	// Prepare I/O pin expander for button press
	i2cIOExp.write(i2cReg.RegInterruptMask, 0xFF);
	i2cIOExp.write(i2cReg.RegInterruptSource, 0xFF);
	i2cIOExp.write(i2cReg.RegSenseLow, 0x00);
	i2cIOExp.write(i2cReg.RegDir, 0xFF);
	i2cIOExp.write(i2cReg.RegData, 0xFF);
	i2cIOExp.write(i2cReg.RegPullUp, 0x04);
	i2cIOExp.write(i2cReg.RegPullDown, 0x00);

	test_log(TEST_CLASS_BUTTON, TEST_INFO, "Waiting for button press.");
	while ((i2cIOExp.read(i2cReg.RegData) & 0x4) != 0)
	    imp.sleep(0.02);
	test_log(TEST_CLASS_BUTTON, TEST_INFO, "Waiting for button release.");
	while ((i2cIOExp.read(i2cReg.RegData) & 0x4) != 0x4);

	// The operator needs to listen to the buzzer sound after pressing and
	// releasing the button, which indicates test pass or fail.
	//
	// HACK ensure test report is submitted even if the button is not functional.
	//
	test_log(TEST_CLASS_BUTTON, TEST_SUCCESS, "Button pressed and released.");
	i2cIOExp.write(i2cReg.RegPullUp, 0x00);

	test_log(TEST_CLASS_BUTTON, TEST_INFO, "**** BUTTON TEST DONE ****");
	testFinish();
    }
    
    function testFinish() {
	test_log(TEST_CLASS_NONE, TEST_INFO, format("Total test time: %dms", hardware.millis() - test_start_time));

	test_log(TEST_CLASS_NONE, TEST_FINAL, "**** TESTS DONE ****");
	
	// enable blink-up in case of a test failure for retesting
	if (!test_ok)
	    imp.enableblinkup(true);
    }

    function testStart(status)
    {
	if (status == SERVER_CONNECTED) {
	    hwPiezo.playSound("test-start", false);
	    //
	    // HACK verify that all devices are reset or reconfigured at the start of the test
	    // as they may hold residual state from a previous test
	    //
	    
	    //
	    // - schedule a watchdog/timeout task that signals test failure should main process get stuck
	    // - log test iteration in nv ram such that it can be retrieved on reboot if the device crashes
	    // - could run 2 or 3 test iterations
	    // - When done with test software, consider translating test report to Chinese such that the factory can
	    //   read and interpret. Alternatively, uniquely enumerate error messages.
	    // - Write a document explaining how each enumerated error message relates to a component failure on the PCB.
	    //   Could translate to Chinese here or ask Flex to do it.
	    
	    factoryTester.test_start_time = hardware.millis();

	    imp.setpowersave(false);

	    test_log(TEST_CLASS_NONE, TEST_INFO, "**** TESTS STARTING ****");
	    test_log(TEST_CLASS_DEV_ID, TEST_SUCCESS, format("Serial number/MAC: %s",imp.getmacaddress()));	
	    test_log(TEST_CLASS_NONE, TEST_INFO, format("Software version: %s",imp.getsoftwareversion()));
	    test_log(TEST_CLASS_NONE, TEST_INFO, format("WiFi network: %s",imp.getssid()));
	    test_log(TEST_CLASS_NONE, TEST_INFO, format("WiFi signal strength (RSSI): %d",imp.rssi()));
	    factoryTester.impPinTest();
	} else {
	    imp.enableblinkup(true);
	    /*
	    imp.onidle(function() {
		    //
		    // count number of WiFi reconnects here in nv ram, fail tests if more 
		    // than a max number of reconnects
		    //
		    //server.log("No WiFi connection, going to sleep again.");
		    //server.sleepfor(1);
		    imp.enableblinkup(true);
		});
*/
	}
	
    }	
}

// Piezo config
hwPiezo <- Piezo(hardware.pin5);
 
factoryTester <- FactoryTester();
connMgr <- ConnectionManager();
connMgr.registerCallback(factoryTester.testStart);
connMgr.init_connections();
	
// HACK 
// Ensure the test can be repeated if it fails somewhere, i.e. ensure
// that blink-up can be redone.