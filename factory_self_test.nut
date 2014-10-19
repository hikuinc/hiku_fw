// Copyright 2014 hiku labs inc. All rights reserved. Confidential.
// Setup the server to behave when we have the no-wifi condition
// server.setsendtimeoutpolicy(RETURN_ON_ERROR, WAIT_TIL_SENT, 30);

local connection_available = false;

testList <- array(0);

//
// I2C addresses and registers
//

// SX1508 or SX1505/2 I2C I/O expanders
// Board is populated with either SX1508, SX1505, or SX1502.
const SX1508ADDR = 0x23;
const SX1505ADDR = 0x21;

// Change this to enable the factory blink-up
// This is the WIFI SSID and password that will be used for factory blink-up
// HACK
const SSID = "2WIRE084";
const PASSWORD = "7890384768";
//const SSID = "hiku-hq";
//const PASSWORD = "broadwayVeer";

// number of seconds after which a watchdog timer flushes
// all outstanding data to the server
const WATCHDOG_FLUSH = 40;

// number of seconds USB charging is turned on for
const CHARGE_DURATION = 2;

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
audio_array <- array(AUDIO_BUFFER_SIZE/2);

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
const TEST_RESULT_INFO = "INFO";
// Test step succeeded
const TEST_RESULT_SUCCESS = "SUCCESS";
// Test step failed, but the test was not essential to provide full
// user-level functionality. For example, an unconnected pin on the Imp that is
// shorted provides an indication of manufacturing problems, but would not impact
// user-level functionality.
const TEST_RESULT_WARNING = "WARNING";
// Test step failed, continue testing. An essential test failed, end result will be FAIL.
const TEST_RESULT_ERROR = "ERROR";
// Test step failed, abort test. An essential test failed and does not allow for
// further testing. For example, if the I2C bus is shorted, all other tests would
// fail, and the Imp could be damaged by continuing the test.
const TEST_RESULT_FATAL = "FATAL";

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
const TEST_CLASS_NONE    = "NONE";
const TEST_CLASS_LED     = "LED";
const TEST_CLASS_IMP_PIN = "IMP_PIN";
const TEST_CLASS_IO_EXP  = "IO_EXP";
const TEST_CLASS_ACCEL   = "ACCEL";
const TEST_CLASS_SCANNER = "SCANNER";
const TEST_CLASS_AUDIO   = "AUDIO";
const TEST_CLASS_CHARGER = "CHARGER";
const TEST_CLASS_BUTTON  = "BUTTON";

//
// Test control flow for communication with backend
//
const TEST_CONTROL_START = "START";
const TEST_CONTROL_UPDATE = "UPDATE";
const TEST_CONTROL_PASS = "PASS";
const TEST_CONTROL_FAIL = "FAIL";

//
// Test IDs identifying individual tests
//
const TEST_ID_OS_VERSION = 100;
const TEST_ID_SSID = 200;
const TEST_ID_RSSI = 300;
const TEST_ID_SCL_SH_VCC = 10000;
const TEST_ID_SDA_SH_VCC = 10100;
const TEST_ID_SCL_PU = 10200;
const TEST_ID_SDA_PU = 10300;
const TEST_ID_EIMP_AUDIO_SH_GND = 10400;
const TEST_ID_EIMP_AUDIO_PD = 10500;
const TEST_ID_PIN6_PD = 10600;
const TEST_ID_PIN6_PU = 10700;
const TEST_ID_PINA_PD = 10800;
const TEST_ID_PINA_PU = 10900;
const TEST_ID_CHARGE_DISABLE_H_PD = 11000;
const TEST_ID_CHARGE_DISABLE_H_PU = 11100;
const TEST_ID_PIND_PD = 11200;
const TEST_ID_PIND_PU = 11300;
const TEST_ID_PINE_PD = 11400;
const TEST_ID_PINE_PU = 11500;
const TEST_ID_IO_EXP_TYPE = 20000;
const TEST_ID_CPU_INT_DEFAULT = 20100;
const TEST_ID_CHARGE_STATUS_L_PU = 20200;
const TEST_ID_SW_VCC_EN_L_PU = 20300;
const TEST_ID_CHARGE_PGOOD_L_PU = 20400;
const TEST_ID_CHARGE_STATUS_L_SH_VCC = 20500;
const TEST_ID_CHARGE_PGOOD_L_SH_VCC = 20600;
const TEST_ID_BUTTON_L_PU = 20700;
const TEST_ID_BUTTON_L_PD = 20800;
const TEST_ID_SCANNER_RESET_L_PD = 20900;
const TEST_ID_SCANNER_RESET_L_PU = 21000;
const TEST_ID_ACCEL_WHO_AM_I = 30000;
const TEST_ID_SX150X_ACCEL_INT = 30100;
const TEST_ID_ACCEL_CPU_INT_BEFORE = 30200;
const TEST_ID_ACCEL_CPU_INT_AFTER = 30300;
const TEST_ID_ACCEL_INT = 30400;
const TEST_ID_SCANNER_UART_NOISE_ERROR = 40000;
const TEST_ID_SCANNER_UART_FRAME_ERROR = 40100;
const TEST_ID_SCANNER_UART_PARITY_ERROR = 40200;
const TEST_ID_SCANNER_UART_OVERRUN_ERROR = 40300;
const TEST_ID_SCANNER_UART_LINE_IDLE = 40400;
const TEST_ID_SCANNER_BARCODE = 40500;
const TEST_ID_AUDIO_BUFFER_OVERRUN = 50000;
const TEST_ID_AUDIO_BUFFER_DATA = 50100;
const TEST_ID_AUDIO_SILENCE_MIN = 50200;
const TEST_ID_AUDIO_SILENCE_MAX = 50300;
const TEST_ID_AUDIO_SILENCE_AMP = 50400;
const TEST_ID_AUDIO_SILENCE_WIFI_AMP = 50500;
const TEST_ID_AUDIO_BUZZER_AMP = 50600;
const TEST_ID_CHARGER_VREF = 60000;
const TEST_ID_CHARGER_BATT_VOLT = 60100;
const TEST_ID_CHARGER_BATT_VOLT_DESIRED = 60200;
const TEST_ID_CHARGER_CHARGE_STATUS_L = 60300;
const TEST_ID_CHARGER_CHARGE_PGOOD_L = 60400;
const TEST_ID_CHARGER_BATT_VOLT_DIFF = 60500;
const TEST_ID_CHARGER_CURRENT = 60600;
const TEST_ID_BUTTON_PRESS = 70000;
const TEST_ID_BUTTON_RELEASE = 70100;
const TEST_ID_GENERIC_IO_EXP_SETUP = 500000;
const TEST_ID_GENERIC_ACCEL_SETUP = 500100;
const TEST_ID_TEST_TIME = 600000;

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
// Handles any I2C device
class I2cDevice
{
    i2cPort = null;
    i2cAddress = null;
    i2cAddress8B = null;
    test_class = null;
    default_test_id = null;

    constructor(port, address, test_cls, default_tst_id)
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
            test_log(TEST_CLASS_NONE, TEST_RESULT_FATAL, "Invalid I2C port");
        }

        // Use the fastest supported clock speed
        i2cPort.configure(CLOCK_SPEED_400_KHZ);

        // Takes a 7-bit I2C address
        i2cAddress = address << 1;
	i2cAddress8B = address;

	test_class = test_cls;
	default_test_id = default_tst_id;
    }

    // Read a byte address
    function read(register, test_id=null)
    {
	if (test_id==null)
	    test_id = default_test_id;
        local data = i2cPort.read(i2cAddress, format("%c", register), 1);
        if(data == null)
        {
	    test_log(test_class, TEST_RESULT_ERROR, format("I2C read, register 0x%x", register), test_id, {address=register});
            // TODO: this should return null, right??? Do better handling.
            // TODO: print the i2c address as part of the error
            return -1;
        }

        return data[0];
    }
    
    // Read a byte address and verify value
    function verify(register, exp_data, test_id=null, log_success=true)
    {
	if (test_id==null)
	    test_id = default_test_id;
	local read_data = read(register, test_id);
	if(read_data != exp_data)
            test_log(test_class, TEST_RESULT_ERROR, format("I2C verify, register 0x%x, expected 0x%x, read 0x%x", register, exp_data, read_data),
	    test_id, {address=register, exp_data=exp_data, read_data=read_data});
	else 
	    if (log_success)
		test_log(test_class, TEST_RESULT_SUCCESS, format("I2C verify, register 0x%x, data 0x%x", register, read_data),
	    test_id, {address=register, read_data=read_data});
    }

    
    // Write a byte address
    function write(register, data, test_id=null)
    {
	if (test_id==null)
	    test_id = default_test_id;
        if(i2cPort.write(i2cAddress, format("%c%c", register, data)) != 0)
            test_log(test_class, TEST_RESULT_ERROR, format("I2C write, register 0x%x", register), test_id, {address=register, write_data=data});
    }

    // Write a byte address, read back register contents and verify if exp_data is not null
    function write_verify(register, data, test_id=null, exp_data=null, log_success=true)
    {
	if (test_id==null)
	    test_id = default_test_id;
	if (exp_data == null)
	    exp_data = data;
	write(register, data, test_id);
	verify(register, exp_data, test_id, log_success);
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
	    test_log(TEST_CLASS_NONE, TEST_RESULT_FATAL, "Invalid pin number on I2C IO expander.");
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
	    test_log(TEST_CLASS_NONE, TEST_RESULT_FATAL, "Invalid signal drive type.");
	}	
    }
    
    // Writes data to pin only. Assumes that pin is already configured as an output.
    function pin_write(pin_num, value) {
	if ((pin_num < 0) || (pin_num > 7) || (value < 0) || (value > 1))
	    test_log(TEST_CLASS_NONE, TEST_RESULT_FATAL, "Invalid pin number or signal value on I2C IO expander.");
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
	    test_log(TEST_CLASS_NONE, TEST_RESULT_FATAL, "Invalid pin number on I2C IO expander.");
	local read_data = read(i2cReg.RegData);
	if (read_data == -1)
	    return read_data;
	else
	    return ((read_data >> pin_num) & 0x01)
    }

    // Configure a pin, drive it as indicated by drive_type, read back the actual value, 
    // quickly re-configure as input (to avoid possible damage to the pin), compare
    // actual vs. expected value.
    function pin_fast_probe(pin_num, expect, drive_type, name, test_id, failure_mode=TEST_RESULT_ERROR) {
	pin_configure(pin_num, drive_type);
	local actual = pin_read(pin_num);
	// Quickly turn off the pin driver after reading as driving the pin may
	// create a short in case of a PCB failure.
	pin_configure(pin_num, DRIVE_TYPE_FLOAT);
	if (actual != expect) {
	    test_log(TEST_CLASS_IO_EXP, failure_mode, format("%s, expected %d, actual %d.", name, expect, actual), 
	    test_id, {pin_expect=expect, pin_actual=actual});
	    test_flush();
	} else
	    test_log(TEST_CLASS_IO_EXP, TEST_RESULT_SUCCESS, format("%s. Value %d.", name, actual),
	    test_id, {pin_actual=actual});

    }
}

function test_flush() {
    if (testList.len() > 0) {
	local result_data_table = {macAddress = imp.getmacaddress(),
	    testControl = TEST_CONTROL_UPDATE,
	    testList = testList}
	agent.send("testresult", result_data_table);
	testList.clear();
    }
}

function test_log(testClass, testResult, testMsg=null, testId=null, jsonData=null) {

    // Create one file with sequence of test messages
    // and individual files for each category.
    // Allows for comparing files for "ERROR" and "SUCCESS"
    // against a known good to determine test PASS or FAIL.
    test_ok = test_ok && (testResult != TEST_RESULT_ERROR) && (testResult != TEST_RESULT_FATAL);

    local testObj = {testClass=testClass,
	testResult=testResult,
	testMsg=testMsg,
	testId=testId,
	jsonData=jsonData};
    
    testList.append(testObj);

    if ((testResult == TEST_RESULT_ERROR) || (testResult == TEST_RESULT_FATAL)) 
	test_flush();
    
    if (testResult == TEST_RESULT_FATAL) 
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

    function pin_validate(pin, expect, name, test_id, failure_mode=TEST_RESULT_ERROR) {
	local actual = pin.read();
	if (actual != expect) {
	    test_log(TEST_CLASS_IMP_PIN, failure_mode, format("%s, expected %d, actual %d.", name, expect, actual), 
	    test_id, {pin_expect=expect, pin_actual=actual});
	    test_flush();
	} else
	    test_log(TEST_CLASS_IMP_PIN, TEST_RESULT_SUCCESS, format("%s. Value %d.", name, actual),
	    test_id, {pin_actual=actual});
    }

    // Configure a pin, drive it as indicated by drive_type, read back the actual value, 
    // quickly re-configure as input (to avoid possible damage to the pin), compare
    // actual vs. expected value.
    function pin_fast_probe(pin, expect, drive_type, name, test_id, failure_mode=TEST_RESULT_ERROR) {
	switch (drive_type) 
	{
	case DRIVE_TYPE_FLOAT: pin.configure(DIGITAL_IN); break;
	case DRIVE_TYPE_PU: pin.configure(DIGITAL_IN_PULLUP); break;
	case DRIVE_TYPE_PD: pin.configure(DIGITAL_IN_PULLDOWN); break;
	case DRIVE_TYPE_LO: pin.configure(DIGITAL_OUT); pin.write(0); break;
	case DRIVE_TYPE_HI: pin.configure(DIGITAL_OUT); pin.write(1); break;
	default:
	    test_log(TEST_CLASS_NONE, TEST_RESULT_FATAL, "Invalid signal drive type.");
	}
	local actual = pin.read();
	// Quickly turn off the pin driver after reading as driving the pin may
	// create a short in case of a PCB failure.
	pin.configure(DIGITAL_IN);
	if (actual != expect) {
	    test_log(TEST_CLASS_IMP_PIN, failure_mode, format("%s, expected %d, actual %d.", name, expect, actual), 
	    test_id, {pin_expect=expect, pin_actual=actual});
	    test_flush();
	} else
	    test_log(TEST_CLASS_IMP_PIN, TEST_RESULT_SUCCESS, format("%s. Value %d.", name, actual),
	    test_id, {pin_actual=actual});
    }

    function impPinTest() {
	//test_log(TEST_CLASS_IMP_PIN, TEST_RESULT_INFO, "**** IMP PIN TESTS STARTING ****");

	// currently unused pins on the Imp
	local pin6 = hardware.pin6;
	local pinA = hardware.pinA;
	local pinD = hardware.pinD;
	local pinE = hardware.pinE;

	// Drive I2C SCL and SDA pins low and check outputs
	// If they cannot be driven low, the I2C is defective and no further
	// tests are possible.
	pin_fast_probe(SCL_OUT, 0, DRIVE_TYPE_LO, "Testing I2C SCL_OUT for short to VCC", TEST_ID_SCL_SH_VCC, TEST_RESULT_FATAL);
	pin_fast_probe(SDA_OUT, 0, DRIVE_TYPE_LO, "Testing I2C SDA_OUT for short to VCC", TEST_ID_SDA_SH_VCC, TEST_RESULT_FATAL);
	// Test external PU resistors on I2C SCL and SDA. If they are not present, the I2C is defective
	// and no further tests are possible.
	pin_fast_probe(SCL_OUT, 1, DRIVE_TYPE_FLOAT, "Testing I2C SCL_OUT for presence of PU resistor", TEST_ID_SCL_PU, TEST_RESULT_FATAL);
	pin_fast_probe(SDA_OUT, 1, DRIVE_TYPE_FLOAT, "Testing I2C SDA_OUT for presence of PU resistor", TEST_ID_SDA_PU, TEST_RESULT_FATAL);

	// Drive buzzer pin EIMP-AUDIO_OUT high and check output
	pin_fast_probe(EIMP_AUDIO_OUT, 1, DRIVE_TYPE_HI, "Testing EIMP-AUDIO_OUT for short to GND", TEST_ID_EIMP_AUDIO_SH_GND);

	// Configure all digital pins on the Imp that have external drivers to floating input.
	CPU_INT.configure(DIGITAL_IN);
	RXD_IN_FROM_SCANNER.configure(DIGITAL_IN);

	// Configure audio and battery voltage inputs as analog
	EIMP_AUDIO_IN.configure(ANALOG_IN);
	BATT_VOLT_MEASURE.configure(ANALOG_IN);

	// Check pin values
	// CPU_INT can only be checked once I2C IO expander has been configured
	// EIMP_AUDIO_IN can only be checked with analog signal from audio amplifier
	pin_fast_probe(EIMP_AUDIO_OUT, 0, DRIVE_TYPE_FLOAT, "Testing EIMP-AUDIO_OUT for presence of PD resistor", TEST_ID_EIMP_AUDIO_PD);
	pin_fast_probe(pin6, 0, DRIVE_TYPE_PD, "Testing open pin6 for floating with PD resistor", TEST_ID_PIN6_PD, TEST_RESULT_WARNING);
	pin_fast_probe(pin6, 1, DRIVE_TYPE_PU, "Testing open pin6 for floating with PU resistor", TEST_ID_PIN6_PU, TEST_RESULT_WARNING);
	// RXD_IN_FROM_SCANNER can only be checked when driven by scanner serial output
	pin_fast_probe(pinA, 0, DRIVE_TYPE_PD, "Testing open pinA for floating with PD resistor", TEST_ID_PINA_PD, TEST_RESULT_WARNING);
	pin_fast_probe(pinA, 1, DRIVE_TYPE_PU, "Testing open pinA for floating with PU resistor", TEST_ID_PINA_PU, TEST_RESULT_WARNING);
	pin_fast_probe(CHARGE_DISABLE_H, 0, DRIVE_TYPE_PD, "Testing CHARGE_DISABLE_H for floating with PD resistor", TEST_ID_CHARGE_DISABLE_H_PD);
	pin_fast_probe(CHARGE_DISABLE_H, 1, DRIVE_TYPE_PU, "Testing CHARGE_DISABLE_H for floating with PU resistor", TEST_ID_CHARGE_DISABLE_H_PU);
	pin_fast_probe(pinD, 0, DRIVE_TYPE_PD, "Testing open pinD for floating with PD resistor", TEST_ID_PIND_PD, TEST_RESULT_WARNING);
	pin_fast_probe(pinD, 1, DRIVE_TYPE_PU, "Testing open pinD for floating with PU resistor", TEST_ID_PIND_PU, TEST_RESULT_WARNING);
	pin_fast_probe(pinE, 0, DRIVE_TYPE_PD, "Testing open pinE for floating with PD resistor", TEST_ID_PINE_PD, TEST_RESULT_WARNING);
	pin_fast_probe(pinE, 1, DRIVE_TYPE_PU, "Testing open pinE for floating with PU resistor", TEST_ID_PINE_PU, TEST_RESULT_WARNING);
	
	// Test neighboring pin pairs for shorts by using a pull-up/pull-down on one
	// and a hard GND/VCC on the other. Check for pin with pull-up/pull-down
	// to not be impacted by neighboring pin.

	//test_log(TEST_CLASS_IMP_PIN, TEST_RESULT_INFO, "**** IMP PIN TESTS DONE ****");
	test_flush();
	ioExpanderTest();
    }

    function ledTest() {
	// HACK
	// Automated LED test TBD
	// Tri-color LED is too far from the photo sensor to sense the
	// emitted light for automated testing.
	//test_log(TEST_CLASS_LED, TEST_RESULT_INFO, "**** LED TEST STARTING ****");
	local lightarray = array(0);
	for (local i=0; i<50; i++) {
	    lightarray.push(hardware.lightlevel());
	    imp.sleep(0.2);
	}
	//test_log(TEST_CLASS_LED, TEST_RESULT_INFO, "**** LET TEST DONE ****");
	test_flush();
	ioExpanderTest();
    }

    function ioExpanderTest() {
	//test_log(TEST_CLASS_IO_EXP, TEST_RESULT_INFO, "**** I/O EXPANDER TESTS STARTING ****");

	local ioexpaddr = SX1508ADDR;

	// Test for presence of either SX1508 or SX1505/2 IO expander
	hardware.configure(I2C_89);
	imp.sleep(i2c_sleep_time);
	hardware.i2c89.configure(CLOCK_SPEED_400_KHZ); // use fastest clock rate
	// Probe SX1508
	local i2cAddress = SX1508ADDR << 1;	
	local data = hardware.i2c89.read(i2cAddress, format("%c", 0x08), 1);
	if (data == null) { 
	    test_log(TEST_CLASS_IO_EXP, TEST_RESULT_INFO, format("SX1508 IO expander not found, I2C error code %d. Trying SX1505/2.", hardware.i2c89.readerror()), TEST_ID_IO_EXP_TYPE, null);
	    hardware.i2c89.disable();
	    // Probe SX1505/2
	    imp.sleep(i2c_sleep_time);
	    i2cAddress = SX1505ADDR << 1;
	    hardware.configure(I2C_89);
	    imp.sleep(i2c_sleep_time);
	    hardware.i2c89.configure(CLOCK_SPEED_400_KHZ); // use fastest clock rate			
	    data = hardware.i2c89.read(i2cAddress, format("%c", 0x00), 1);
	    if(data == null) {
		test_log(TEST_CLASS_IO_EXP, TEST_RESULT_FATAL, format("Found neither SX1508 nor SX1505/2 IO expander, I2C error code %d", hardware.i2c89.readerror()), TEST_ID_IO_EXP_TYPE, {io_exp_type="NOT_FOUND"});
	    }
	    else {
		test_log(TEST_CLASS_IO_EXP, TEST_RESULT_SUCCESS, "Found SX1505/2 IO expander", TEST_ID_IO_EXP_TYPE, {io_exp_type="SX1505"});
		ioexpaddr = SX1505ADDR;
	    }}
	else
	    {
	    // We detected SX1508
	    test_log(TEST_CLASS_IO_EXP, TEST_RESULT_SUCCESS, "Found SX1508 IO expander", TEST_ID_IO_EXP_TYPE, {io_exp_type="SX1508"});
	    ioexpaddr = SX1508ADDR;
	}
	hardware.i2c89.disable();

	// at this stage if the ioexpaddr is not chosen, then default to SX1508ADDR
	i2cReg = (ioexpaddr == SX1505ADDR) ? sx1505reg:sx1508reg;
	i2cIOExp = IoExpander(I2C_89, ioexpaddr, TEST_CLASS_IO_EXP, TEST_ID_GENERIC_IO_EXP_SETUP);

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
	pin_validate(CPU_INT, 0, "Check CPU_INT to be 0 with SX1508/5/2 default config", TEST_ID_CPU_INT_DEFAULT);

	// CHARGE_STATUS_L, CHARGE_PGOOD_L, SW_VCC_EN_OUT_L should be pulled high through
	// external pull-ups when not in use
	i2cIOExp.pin_fast_probe(CHARGE_STATUS_L, 1, DRIVE_TYPE_FLOAT, "Testing CHARGE_STATUS_L for presence of PU resistor", TEST_ID_CHARGE_STATUS_L_PU);
	i2cIOExp.pin_fast_probe(SW_VCC_EN_L, 1, DRIVE_TYPE_FLOAT, "Testing SW_VCC_EN_L/SW_VCC_EN_OUT_L for presence of PU resistor", TEST_ID_SW_VCC_EN_L_PU);
	i2cIOExp.pin_fast_probe(CHARGE_PGOOD_L, 1, DRIVE_TYPE_FLOAT, "Testing CHARGE_PGOOD_L for presence of PU resistor", TEST_ID_CHARGE_PGOOD_L_PU);

	// CHARGE_STATUS_L and CHARGE_PGOOD_L are open-drain on the BQ24072; should be 
	// able to pull them low from the I/O expander    
	i2cIOExp.pin_fast_probe(CHARGE_STATUS_L, 0, DRIVE_TYPE_LO, "Testing CHARGE_STATUS_L for short to VCC", TEST_ID_CHARGE_STATUS_L_SH_VCC);
	i2cIOExp.pin_fast_probe(CHARGE_PGOOD_L, 0, DRIVE_TYPE_LO, "Testing CHARGE_PGOOD_L for short to VCC", TEST_ID_CHARGE_PGOOD_L_SH_VCC);

	// BUTTON_L floats when the button is not pressed; should be able to pull it high
	// or low with a pull-up/pull-down
	i2cIOExp.pin_fast_probe(BUTTON_L, 1, DRIVE_TYPE_PU, "Testing BUTTON_L if it can be pulled up", TEST_ID_BUTTON_L_PU);
	i2cIOExp.pin_fast_probe(BUTTON_L, 0, DRIVE_TYPE_PD, "Testing BUTTON_L if it can be pulled down", TEST_ID_BUTTON_L_PD);
	// SCANNER_RESET_L floats; should be able to pull it low with a pull-down
	i2cIOExp.pin_fast_probe(SCANNER_RESET_L, 0, DRIVE_TYPE_PD, "Testing SCANNER_RESET_L if it can be pulled down", TEST_ID_SCANNER_RESET_L_PD);
	// should be able to pull it high with a pull-up when the scanner is powered
	i2cIOExp.pin_configure(SW_VCC_EN_L, DRIVE_TYPE_LO);
	i2cIOExp.pin_fast_probe(SCANNER_RESET_L, 1, DRIVE_TYPE_PU, "Testing SCANNER_RESET_L if it can be pulled up", TEST_ID_SCANNER_RESET_L_PU);
	i2cIOExp.pin_configure(SW_VCC_EN_L, DRIVE_TYPE_FLOAT);
	test_flush();

	//test_log(TEST_CLASS_IO_EXP, TEST_RESULT_INFO, "**** I/O EXPANDER TESTS DONE ****");
	accelerometerTest();
    }

    function accelerometerTest() {
	//test_log(TEST_CLASS_ACCEL, TEST_RESULT_INFO, "**** ACCELEROMETER TESTS STARTING ****");

	// create an I2C device for the LIS3DH accelerometer
	local i2cAccel = I2cDevice(I2C_89, ACCELADDR, TEST_CLASS_ACCEL, TEST_ID_GENERIC_ACCEL_SETUP);

	// test presence of the accelerometer by reading the WHO_AM_I register
	i2cAccel.verify(0x0F, 0x33, TEST_ID_ACCEL_WHO_AM_I);

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
	pin_validate(CPU_INT, 0, "Verify CPU_INT at 0 before interrupt", TEST_ID_ACCEL_CPU_INT_BEFORE);

	// enable motion interrupt
	i2cAccel.write(ACCEL_CTRL_REG3, 0x40);
	i2cAccel.write(ACCEL_INT1_CFG, 0x3F);
	// enable self test mode to simulate motion; wait for motion to be registered
	i2cAccel.write(ACCEL_CTRL_REG4, 0x02); // CTRL_REG4: Enable self test
	imp.sleep(0.05);

	// verify that the accelerometer triggered the interrupt on signal CPU_INT on the Imp,
	// in register RegInterruptSource on the SX1508/5/2, and in register INT1_SRC on accelerometer
	pin_validate(CPU_INT, 1, "Verify CPU_INT at 1 after interrupt", TEST_ID_ACCEL_CPU_INT_AFTER);
	i2cIOExp.verify(i2cReg.RegInterruptSource, 0x01, TEST_ID_SX150X_ACCEL_INT);
	i2cAccel.verify(ACCEL_INT1_SRC, 0x6A, TEST_ID_ACCEL_INT); 

	// disable self test and interrupts, clear interrupt bit
	i2cAccel.write(ACCEL_CTRL_REG4, 0x00); 
	i2cAccel.write(ACCEL_CTRL_REG3, 0x00); 
	i2cAccel.write(ACCEL_INT1_CFG, 0x00);
	i2cAccel.read(ACCEL_INT1_SRC); 

	//test_log(TEST_CLASS_ACCEL, TEST_RESULT_INFO, "**** ACCELEROMETER TESTS DONE ****");
	test_flush();
	scannerTest();
    }

    function scannerTest() {
	//test_log(TEST_CLASS_SCANNER, TEST_RESULT_INFO, "**** SCANNER TESTS STARTING ****");

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
	SCANNER_UART.configure(38400, 8, PARITY_NONE, 1, NO_CTSRTS | NO_TX);
	// set SCANNER_TRIGGER_OUT_L (0x20) low to turn the scanner on
	regData = regData & 0xDF;
	i2cIOExp.write(i2cReg.RegData, regData);

	local uart_flags = SCANNER_UART.flags();
	local scanWaitCount = 0;
	// try scanning for up to 2s
	while (((uart_flags & READ_READY)==0) && (scanWaitCount < 40)) {
	    scanWaitCount = scanWaitCount + 1;
	    imp.sleep(0.05);
	    uart_flags = SCANNER_UART.flags();
	}
	if (uart_flags & NOISE_ERROR)
	    test_log(TEST_CLASS_SCANNER, TEST_RESULT_ERROR, "NOISE_ERROR bit set on UART", TEST_ID_SCANNER_UART_NOISE_ERROR);
	if (uart_flags & FRAME_ERROR)
	    test_log(TEST_CLASS_SCANNER, TEST_RESULT_ERROR, "FRAME_ERROR bit set on UART", TEST_ID_SCANNER_UART_FRAME_ERROR);
	if (uart_flags & PARITY_ERROR)
	    test_log(TEST_CLASS_SCANNER, TEST_RESULT_ERROR, "PARITY_ERROR bit set on UART", TEST_ID_SCANNER_UART_PARITY_ERROR);
	if (uart_flags & OVERRUN_ERROR)
	    test_log(TEST_CLASS_SCANNER, TEST_RESULT_ERROR, "OVERRUN_ERROR bit set on UART", TEST_ID_SCANNER_UART_OVERRUN_ERROR);
	if (uart_flags & LINE_IDLE)
	    test_log(TEST_CLASS_SCANNER, TEST_RESULT_INFO, "LINE_IDLE bit set on UART", TEST_ID_SCANNER_UART_LINE_IDLE);

	// after 20ms sleep (from the while loop) the full string should be available at
	// both 9600 and 38400 UART bit rates
	imp.sleep(0.02);	
	local scan_string = SCANNER_UART.readstring();

	// turn UART and scanner off, set SW_VCC_EN_OUT_L, SCANNER_TRIGGER_OUT_L, SCANNER_RESET_L to inputs
	SCANNER_UART.disable();
	i2cIOExp.write(i2cReg.RegDir, i2cIOExp.read(i2cReg.RegDir) | 0x70);

	if (scan_string == TEST_REFERENCE_BARCODE) 
	    test_log(TEST_CLASS_SCANNER, TEST_RESULT_SUCCESS, format("Scanned %s", scan_string), TEST_ID_SCANNER_BARCODE, {barcode=scan_string});
	else
	    test_log(TEST_CLASS_SCANNER, TEST_RESULT_ERROR, format("Scanned %s, expected %s", scan_string, TEST_REFERENCE_BARCODE), 
	    TEST_ID_SCANNER_BARCODE, {barcode=scan_string});
	
	//test_log(TEST_CLASS_SCANNER, TEST_RESULT_INFO, "**** SCANNER TESTS DONE ****");
	test_flush();
	audioTest();
    }

    function audioTest() {
	i2cIOExp.pin_configure(SW_VCC_EN_L, DRIVE_TYPE_LO);
	//test_log(TEST_CLASS_AUDIO, TEST_RESULT_INFO, "**** AUDIO TESTS STARTING ****");
	// Flush all data to the server to not introduce WiFi
	// noise into audio recording.
	server.flush(2);
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
	    test_log(TEST_CLASS_AUDIO, TEST_RESULT_ERROR, "Audio sampler buffer overrun.", TEST_ID_AUDIO_BUFFER_OVERRUN);
	    audioCurrentTest = AUDIO_TEST_DONE;
	}
	else {
	    audioBufCount++;
	    if (audioBufCount == 4) 
		hardware.sampler.stop();
	    if (audioCurrentTest == AUDIO_TEST_SILENCE_WIFI)
		test_flush();
	    for (local i=0; i<length/2; i++)
		audio_array[i]=(buffer[2*i+1] << 4) + (buffer[2*i] >> 4);
	    test_log(TEST_CLASS_AUDIO, TEST_RESULT_INFO, "Audio buffer data.", TEST_ID_AUDIO_BUFFER_DATA, 
		{audioBufCount=audioBufCount, audioCurrentTest=audioCurrentTest, data=audio_array, size=length/2});
	    // Average over AUDIO_NUM_VALUES min and max values
	    // to determine signal amplitude; avoids pass/fail based on outliers.
	    local audio_val;
	    for (local i=0; i<length/2; i++) {
		audio_val = audio_array[i];
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
			test_log(TEST_CLASS_AUDIO, TEST_RESULT_SUCCESS, format("Audio reading min %d.", audioMin),
			TEST_ID_AUDIO_SILENCE_MIN, {audioMin=audioMin, requiredMin=AUDIO_MID-AUDIO_MID_VARIANCE});
		    else
			test_log(TEST_CLASS_AUDIO, TEST_RESULT_ERROR, format("Audio reading min %d, required >%d.", audioMin, AUDIO_MID-AUDIO_MID_VARIANCE),
			TEST_ID_AUDIO_SILENCE_MIN, {audioMin=audioMin, requiredMin=AUDIO_MID-AUDIO_MID_VARIANCE});
		    if (audioMax < (AUDIO_MID+AUDIO_MID_VARIANCE))
			test_log(TEST_CLASS_AUDIO, TEST_RESULT_SUCCESS, format("Audio reading max %d.", audioMax),
			TEST_ID_AUDIO_SILENCE_MAX, {audioMax=audioMax, requiredMax=AUDIO_MID+AUDIO_MID_VARIANCE});
		    else
			test_log(TEST_CLASS_AUDIO, TEST_RESULT_ERROR, format("Audio reading max %d, required <%d.", audioMax, AUDIO_MID+AUDIO_MID_VARIANCE),
			TEST_ID_AUDIO_SILENCE_MAX, {audioMax=audioMax, requiredMax=AUDIO_MID+AUDIO_MID_VARIANCE});
		    if ((audioMax-audioMin) < AUDIO_SILENCE_AMP_NO_WIFI)
			test_log(TEST_CLASS_AUDIO, TEST_RESULT_SUCCESS, format("Audio amplitude recording silence %d (without WiFi).", audioMax-audioMin), 
			TEST_ID_AUDIO_SILENCE_AMP, {audioAmp=audioMax-audioMin, requiredAmp=AUDIO_SILENCE_AMP_NO_WIFI});
		    else
			test_log(TEST_CLASS_AUDIO, TEST_RESULT_ERROR, format("Audio amplitude recording silence %d (without WiFi), required <%d.", audioMax-audioMin, AUDIO_SILENCE_AMP_NO_WIFI), 
			TEST_ID_AUDIO_SILENCE_AMP, {audioAmp=audioMax-audioMin, requiredAmp=AUDIO_SILENCE_AMP_NO_WIFI});
		    audioCurrentTest = AUDIO_TEST_SILENCE_WIFI;
		    break;
		case AUDIO_TEST_SILENCE_WIFI:
		    if ((audioMax-audioMin) < AUDIO_SILENCE_AMP_WIFI)
			test_log(TEST_CLASS_AUDIO, TEST_RESULT_SUCCESS, format("Audio amplitude recording silence %d (with WiFi).", audioMax-audioMin), 
			TEST_ID_AUDIO_SILENCE_WIFI_AMP, {audioAmp=audioMax-audioMin, requiredAmp=AUDIO_SILENCE_AMP_WIFI});
		    else
			test_log(TEST_CLASS_AUDIO, TEST_RESULT_ERROR, format("Audio amplitude recording silence %d (with WiFi), required <%d.", audioMax-audioMin, AUDIO_SILENCE_AMP_WIFI), 
			TEST_ID_AUDIO_SILENCE_WIFI_AMP, {audioAmp=audioMax-audioMin, requiredAmp=AUDIO_SILENCE_AMP_WIFI});
		    audioCurrentTest = AUDIO_TEST_BUZZER;
		    hwPiezo.playSound("one-khz");
		    break;
		case AUDIO_TEST_BUZZER:
		    if (((audioMax-audioMin) > AUDIO_BUZZER_AMP_MIN) && ((audioMax-audioMin) < AUDIO_BUZZER_AMP_MAX))
			test_log(TEST_CLASS_AUDIO, TEST_RESULT_SUCCESS, format("Audio amplitude recording buzzer %d.", audioMax-audioMin), 
			TEST_ID_AUDIO_BUZZER_AMP, {audioAmp=audioMax-audioMin, requiredAmpMin=AUDIO_BUZZER_AMP_MIN, requiredAmpMax=AUDIO_BUZZER_AMP_MAX});
		    if ((audioMax-audioMin) <= AUDIO_BUZZER_AMP_MIN)
			test_log(TEST_CLASS_AUDIO, TEST_RESULT_ERROR, format("Audio amplitude recording buzzer %d, required >%d.", audioMax-audioMin, AUDIO_BUZZER_AMP_MIN), 
			TEST_ID_AUDIO_BUZZER_AMP, {audioAmp=audioMax-audioMin, requiredAmp=AUDIO_BUZZER_AMP_MIN});
		    if ((audioMax-audioMin) >= AUDIO_BUZZER_AMP_MAX)
			test_log(TEST_CLASS_AUDIO, TEST_RESULT_ERROR, format("Audio amplitude recording buzzer %d, required <%d.", audioMax-audioMin, AUDIO_BUZZER_AMP_MAX), 
			TEST_ID_AUDIO_BUZZER_AMP, {audioAmp=audioMax-audioMin, requiredAmp=AUDIO_BUZZER_AMP_MAX});
		    audioCurrentTest = AUDIO_TEST_DONE;
		    break;
		case AUDIO_TEST_DONE:
		    break;
		default:
		    test_log(TEST_CLASS_AUDIO, TEST_RESULT_ERROR, "Invalid audio test.");
		    audioCurrentTest = AUDIO_TEST_DONE;
		    break;
		}
		if (audioCurrentTest != AUDIO_TEST_DONE) {
		    // Flush all data to the server to not introduce WiFi
		    // noise into the following recording.
		    server.flush(2);
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
	    //test_log(TEST_CLASS_AUDIO, TEST_RESULT_INFO, "**** AUDIO TESTS DONE ****");
	    test_flush();
	    factoryTester.chargerTest();
	}
    }
 
    function chargerTest() {
	// turn off microphone/scanner after audio test
	i2cIOExp.pin_configure(SW_VCC_EN_L, DRIVE_TYPE_FLOAT);
	
	//test_log(TEST_CLASS_CHARGER, TEST_RESULT_INFO, "**** CHARGER TESTS STARTING ****");

	local vref_voltage = hardware.voltage();
	if ((vref_voltage > VREF_MIN) && (vref_voltage < VREF_MAX))
	    test_log(TEST_CLASS_CHARGER, TEST_RESULT_SUCCESS, format("Reference voltage VREF %fV.", vref_voltage),
	    TEST_ID_CHARGER_VREF, {vref_voltage=vref_voltage});
	else
	    test_log(TEST_CLASS_CHARGER, TEST_RESULT_ERROR, format("Reference voltage VREF %fV out of range, required %fV<VREF<%fV.", vref_voltage, VREF_MIN, VREF_MAX),
	    TEST_ID_CHARGER_VREF, {vref_voltage=vref_voltage, vref_min=VREF_MIN, vref_max=VREF_MAX});
	    
	local bat_acc = 0;
	for (local i = 0; i < BATT_ADC_SAMPLES; i++)
    	    bat_acc += (BATT_VOLT_MEASURE.read() >> 4) & 0xFFF;

	local batt_voltage = (bat_acc/BATT_ADC_SAMPLES) * BATT_ADC_RDIV;

	// check battery voltage to be in allowable range
	if (batt_voltage > BATT_MIN_VOLTAGE) {
	    if (batt_voltage < BATT_MAX_VOLTAGE)
		test_log(TEST_CLASS_CHARGER, TEST_RESULT_SUCCESS, format("Battery voltage %fV.", batt_voltage), 
		TEST_ID_CHARGER_BATT_VOLT, {batt_voltage=batt_voltage});
	    else
		test_log(TEST_CLASS_CHARGER, TEST_RESULT_ERROR, format("Battery voltage %fV higher than allowed %fV.", batt_voltage, BATT_MAX_VOLTAGE), 
		TEST_ID_CHARGER_BATT_VOLT, {batt_voltage=batt_voltage, batt_max_voltage=BATT_MAX_VOLTAGE});
	} else 
	    test_log(TEST_CLASS_CHARGER, TEST_RESULT_ERROR, format("Battery voltage %fV lower than allowed %fV.", batt_voltage, BATT_MIN_VOLTAGE), 
		TEST_ID_CHARGER_BATT_VOLT, {batt_voltage=batt_voltage, batt_min_voltage=BATT_MIN_VOLTAGE});

	// check battery voltage to be in desirable range for shipment to customers
	if (batt_voltage < BATT_MIN_WARN_VOLTAGE)
	    test_log(TEST_CLASS_CHARGER, TEST_RESULT_WARNING, format("Battery voltage %fV below desired %fV.", batt_voltage, BATT_MIN_WARN_VOLTAGE),
	    TEST_ID_CHARGER_BATT_VOLT, {batt_voltage=batt_voltage, batt_min_warn_voltage=BATT_MIN_WARN_VOLTAGE});

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
	    test_log(TEST_CLASS_CHARGER, TEST_RESULT_ERROR, "CHARGE_STATUS_L not low when USB charging.", TEST_ID_CHARGER_CHARGE_STATUS_L);
	if (charge_pgood)
	    test_log(TEST_CLASS_CHARGER, TEST_RESULT_ERROR, "CHARGE_PGOOD_L not low when USB charging.", TEST_ID_CHARGER_CHARGE_PGOOD_L);

	// wait for the battery voltage to stabilize with charger switched on
	imp.sleep(0.1);

	bat_acc = 0;
	for (local i = 0; i < BATT_ADC_SAMPLES; i++)
    	    bat_acc += (BATT_VOLT_MEASURE.read() >> 4) & 0xFFF;

	local charge_voltage = (bat_acc/BATT_ADC_SAMPLES) * BATT_ADC_RDIV;
	local volt_diff = charge_voltage - batt_voltage;

	if (volt_diff > CHARGE_MIN_INCREASE)
	    test_log(TEST_CLASS_CHARGER, TEST_RESULT_SUCCESS, format("Battery voltage when charging %fV greater than before charging.", volt_diff),
	    TEST_ID_CHARGER_BATT_VOLT_DIFF, {volt_diff=volt_diff});
	else
	    test_log(TEST_CLASS_CHARGER, TEST_RESULT_ERROR, format("Battery voltage difference %fV to before charging, requiring greater %fV.", volt_diff, CHARGE_MIN_INCREASE),
	    TEST_ID_CHARGER_BATT_VOLT_DIFF, {volt_diff=volt_diff, charge_min_increase=CHARGE_MIN_INCREASE});
	
	test_flush();
	//test_log(TEST_CLASS_CHARGER, TEST_RESULT_INFO, "**** CHARGER TESTS DONE ****");
	buttonTest();
    }

    function buttonTest() {
	//test_log(TEST_CLASS_BUTTON, TEST_RESULT_INFO, "**** BUTTON TEST STARTING ****");

	// Prepare I/O pin expander for button press
	i2cIOExp.write(i2cReg.RegInterruptMask, 0xFF);
	i2cIOExp.write(i2cReg.RegInterruptSource, 0xFF);
	i2cIOExp.write(i2cReg.RegSenseLow, 0x00);
	i2cIOExp.write(i2cReg.RegDir, 0xFF);
	i2cIOExp.write(i2cReg.RegData, 0xFF);
	i2cIOExp.write(i2cReg.RegPullUp, 0x04);
	i2cIOExp.write(i2cReg.RegPullDown, 0x00);

	while ((i2cIOExp.read(i2cReg.RegData) & 0x4) != 0)
	    imp.sleep(0.02);
        test_log(TEST_CLASS_BUTTON, TEST_RESULT_SUCCESS, "Button pressed.", TEST_ID_BUTTON_PRESS);
	while ((i2cIOExp.read(i2cReg.RegData) & 0x4) != 0x4);
        test_log(TEST_CLASS_BUTTON, TEST_RESULT_SUCCESS, "Button released.", TEST_ID_BUTTON_RELEASE);

	// The operator needs to listen to the buzzer sound after pressing and
	// releasing the button, which indicates test pass or fail.
	i2cIOExp.write(i2cReg.RegPullUp, 0x00);

	//test_log(TEST_CLASS_BUTTON, TEST_RESULT_INFO, "**** BUTTON TEST DONE ****");
	testFinish();
    }
    
    function testFinish() {
	local test_time = hardware.millis() - test_start_time;
	test_log(TEST_CLASS_NONE, TEST_RESULT_INFO, format("Total test time: %dms", test_time), TEST_ID_TEST_TIME, {test_time=test_time});

	local result_data_table = {macAddress = imp.getmacaddress(),
	    testControl = test_ok ? TEST_CONTROL_PASS : TEST_CONTROL_FAIL,
	    testList = testList};
	agent.send("testresult", result_data_table);
	testList.clear()

	if (test_ok)
	    hwPiezo.playSound("test-pass", false)
	else {
	    hwPiezo.playSound("test-fail", false);
	    // enable blink-up in case of a test failure for retesting
	    //imp.enableblinkup(true);
	}
	// HACK
	// HACK
	// HACK
	// retest automatically for software dev
	imp.enableblinkup(true);
    }

    function testStart(status)
    {
	// HACK
	// allow for blinkup during test to reprogram on code failure
	imp.enableblinkup(true);
	if (status == SERVER_CONNECTED) {
	    hwPiezo.playSound("test-start", false);
	    //
	    // HACK verify that all devices are reset or reconfigured at the start of the test
	    // as they may hold residual state from a previous test
	    //
	    //
	    // - log test iteration in nv ram such that it can be retrieved on reboot if the device crashes
	    // - could run 2 or 3 test iterations
	    // - When done with test software, consider translating test report to Chinese such that the factory can
	    //   read and interpret. Alternatively, uniquely enumerate error messages.
	    // - Write a document explaining how each enumerated error message relates to a component failure on the PCB.
	    //   Could translate to Chinese here or ask Flex to do it.

	    testList.clear();
	    local result_data_table = {macAddress = imp.getmacaddress(),
		testControl = TEST_CONTROL_START,
		testList = testList};
	    agent.send("testresult", result_data_table);
	    
	    factoryTester.test_start_time = hardware.millis();
	    // flush all data to the server using a watchdog timer in case the
	    // test gets stuck and there is data remaining
	    imp.wakeup(WATCHDOG_FLUSH, test_flush)

	    //test_log(TEST_CLASS_NONE, TEST_RESULT_INFO, "**** TESTS STARTING ****");
	    local os_version = imp.getsoftwareversion();
	    test_log(TEST_CLASS_NONE, TEST_RESULT_INFO, format("OS version: %s",os_version), TEST_ID_OS_VERSION, {os_version=os_version});
	    local ssid = imp.getssid();
	    test_log(TEST_CLASS_NONE, TEST_RESULT_INFO, format("WiFi network: %s",ssid), TEST_ID_SSID, {ssid=ssid});
	    local rssi = imp.rssi();
	    test_log(TEST_CLASS_NONE, TEST_RESULT_INFO, format("WiFi signal strength (RSSI): %d",rssi), TEST_ID_RSSI, {rssi=rssi});
	    test_flush();
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

function factoryOnIdle() {
    imp.onidle(function(){
	    BLINKUP_BUTTON.configure(DIGITAL_IN_WAKEUP, buttonCallback);
	    BLINKUP_LED.configure(DIGITAL_IN_PULLDOWN);
	});
}

function buttonCallback()
{
    // Disable any further callbacks until blink-up is done
    BLINKUP_BUTTON.configure(DIGITAL_IN);
    // Check if button is pressed
    if (BLINKUP_BUTTON.read() == 1) {
	USB_POWER.write(0);
	BLINKUP_LED.configure(DIGITAL_OUT);
    	BLINKUP_LED.write(0);
	// turn of red LED if it was on from a previous test failure
	FAILURE_LED_RED.write(0);
	// briefly blink green LED to indicate start of test
	SUCCESS_LED_GREEN.write(0);
	imp.sleep(0.1);
	SUCCESS_LED_GREEN.write(1);
	imp.sleep(0.1);
	SUCCESS_LED_GREEN.write(0);
    	server.log("Factory blink-up started");
    	server.factoryblinkup(SSID,PASSWORD, BLINKUP_LED, BLINKUP_ACTIVEHIGH | BLINKUP_FAST);
	//
	// HACK
	// implement synchronization through backend for better timing
	imp.wakeup(20, function() {
		local charging_ok = false;
		USB_POWER.write(1);
		imp.sleep(0.1);
		// Check PWRDG to be 1. If it's 0 there is a short or over-current on the USB
		if (USB_PWRGD.read() == 1) {
		    charging_ok = true;
		    server.log("Charging current in range.");
		    //HACK
		    //match devices through different barcodes in compartments 0..3
		    //test_log(TEST_CLASS_CHARGER, TEST_RESULT_SUCCESS, "Charging current below 700mA.", TEST_ID_CHARGER_CURRENT);
		} else {
		    charging_ok = false;
		    server.log("Error: Charging current exceeds 700mA on at least one device.");
		    //test_log(TEST_CLASS_CHARGER, TEST_RESULT_ERROR, "Charging current exceeds 700mA.", TEST_ID_CHARGER_CURRENT);
		}
		//test_flush();
		imp.sleep(CHARGE_DURATION);
		USB_POWER.write(0);
    		server.log("USB charger off");
		if (charging_ok)
		    SUCCESS_LED_GREEN.write(1);
		else
		    FAILURE_LED_RED.write(1);		    
    		server.log("Factory blink-up done");
		// HACK
		// HACK
		// HACK
		// turn on charging again so DUT doesn't die while writing test software
		USB_POWER.write(1);
		factoryOnIdle();
	    });
    } else 
	factoryOnIdle();
}


function init()
{
    imp.setpowersave(false);
    local board_type = imp.environment();
    if (ENVIRONMENT_CARD == board_type ) {
	//
	// Pin assignment on the factory Imp
	//
	BLINKUP_BUTTON      <- hardware.pin1;
	USB_POWER           <- hardware.pin2;
	BLINKUP_LED         <- hardware.pin5;
	USB_PWRGD           <- hardware.pin7;
	FAILURE_LED_RED     <- hardware.pin8;
	SUCCESS_LED_GREEN   <- hardware.pin9;

	// configure LED outputs
	SUCCESS_LED_GREEN.configure(DIGITAL_OUT);
	FAILURE_LED_RED.configure(DIGITAL_OUT);
	BLINKUP_LED.configure(DIGITAL_OUT);
    	BLINKUP_LED.write(0);

	// turn off USB power initially
	USB_POWER.configure(DIGITAL_OUT);
	USB_POWER.write(0);

	// PWRGD on MCP1825 is open drain, needs a pull-up resistor
	USB_PWRGD.configure(DIGITAL_IN_PULLUP);
	
	factoryOnIdle();
    } else if (ENVIRONMENT_MODULE == board_type) {
	//
	// Pin assignment on the hiku DUT Imp
	//
	CPU_INT             <- hardware.pin1;
	EIMP_AUDIO_IN       <- hardware.pin2; // "EIMP-AUDIO_IN" in schematics
	EIMP_AUDIO_OUT      <- hardware.pin5; // "EIMP-AUDIO_OUT" in schematics
	RXD_IN_FROM_SCANNER <- hardware.pin7;
	SCL_OUT             <- hardware.pin8;
	SDA_OUT             <- hardware.pin9;
	BATT_VOLT_MEASURE   <- hardware.pinB;
	CHARGE_DISABLE_H    <- hardware.pinC;
	SCANNER_UART        <- hardware.uart57;
	
	// Piezo config
	hwPiezo <- Piezo(EIMP_AUDIO_OUT);

	factoryTester <- FactoryTester();
	factoryTester.testStart(SERVER_CONNECTED);
    }
}

//**********************************************************************
// main
// Only run in the factory with a specified SSID
if (imp.getssid() == SSID)
    init();
else
    server.log("ERROR: SSID does not match pre-configured factory SSID");

// HACK 
// Ensure the test can be repeated if it fails somewhere, i.e. ensure
// that blink-up can be redone.