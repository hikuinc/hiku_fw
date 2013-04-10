// Copyright 2013 Katmandu Technology, Inc. All rights reserved. Confidential.


// Consts and enums
const cFirmwareVersion = "0.5.0"
const cButtonTimeout = 6;  // in seconds
const cDelayBeforeDeepSleep = 10.0;  // in seconds
//const cDelayBeforeDeepSleep = 3600.0;  // in seconds
const cDeepSleepDuration = 86380.0;  // in seconds (24h - 20s)

enum DeviceState
/*
                           ---> SCAN_CAPTURED ------>
                          /                          \
    IDLE ---> SCAN_RECORD ---> BUTTON_TIMEOUT -->     \
                          \                      \     \
                           -------------------> BUTTON_RELEASED ---> IDLE
*/
{
    IDLE,             // 0: Not recording or processing data
    SCAN_RECORD,      // 1: Scanning and recording audio
    SCAN_CAPTURED,    // 2: Processing scan data
    BUTTON_TIMEOUT,   // 3: Timeout limit reached while holding button
    BUTTON_RELEASED,  // 4: Button released, may have audio to send
}


// Globals
gDeviceState <- null; // Hiku device current state

gAudioBufferOverran <- false; // True if an overrun occurred
gAudioChunkCount <- 0; // Number of audio buffers (chunks) captured
gAudioBufferSize <- 2000; // Size of each audio buffer 
gAudioSampleRate <- 16000; // in Hz

// Workaround to capture last buffer after sampler is stopped
gSamplerStopping <- false; 
gLastSamplerBuffer <- null; 
gLastSamplerBufLen <- 0; 
gAudioTimer <- 0;

// TODO: get rid of this
gPin1HandlerSet <- false;

// Each 1k of buffer will hold 1/16 of a second of audio, or 63ms.
// TODO: A-law sampler does not return partial buffers. This means that up to 
// the last buffer size of data is dropped. Filed issue with IE here: 
// http://forums.electricimp.com/discussion/780/. So keep buffers small. 
buf1 <- blob(gAudioBufferSize);
buf2 <- blob(gAudioBufferSize);
buf3 <- blob(gAudioBufferSize);


//======================================================================
// Handles all audio output
class Piezo
{
    // The hardware pin controlling the piezo 
    pin = null;

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

    //**********************************************************
    constructor(hwPin)
    {
        pin = hwPin;

        // Configure pin
        pin.configure(DIGITAL_OUT);
        pin.write(0); // Turn off piezo by default

        // TODO: remove tones for sleep and startup, unless debugging
        tonesParamsList = {
            // [[period, duty cycle, duration], ...]
            "success": [[noteE5, dc, longTone], [noteE6, dc, shortTone]],
            "failure": [/*silence*/],
            "unknown-upc": [[noteB4, dc, shortTone], [noteB4, 0, shortTone], 
            [noteB4, dc, shortTone], [noteB4, 0, shortTone], 
            [noteB4, dc, shortTone], [noteB4, 0, shortTone]],
            "timeout": [/*silence*/],
            "sleep":   [[noteE5, dc, longTone], [noteB4, dc, shortTone]],
            "startup": [[noteB4, dc, longTone], [noteE5, dc, shortTone]],
        };
    }

    //**********************************************************
    // Play a tone (a set of notes).  Defaults to asynchronous
    // playback, but also supports synchronous via busy waits
    function playSound(tone = "success", async = true) 
    {
        //TODO HWDEBUG: remove after piezo HW fixed
        server.log(format("(skipping playing %s tone)", tone));
        return;

        // Handle invalid tone values
        if (!(tone in tonesParamsList))
        {
            server.log(format("Error: unknown tone \"%s\"", tone));
            return;
        }

        // Handle "silent" tones
        if (tonesParamsList[tone].len() == 0)
        {
            return;
        }

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
            server.log("Error: tried to play null tone");
            return;
        }

        // Play the next note, if any
        if (tonesParamsList[currentTone].len() > currentToneIdx)
        {
            local params = tonesParamsList[currentTone][currentToneIdx];
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
// 
// Example usage: set up a timer to call dosomething() in 10 minutes
//   doSomethingTimer = CancellableTimer(60*10, function(){dosomething();});
//   doSomethingTimer.enable()
//   doSomethingTimer.disable()
class CancellableTimer
{
    enabled = null;
    startTime = null; // Timer starting time, in milliseconds
    duration = null; // How long the timer should run before doing the action
    actionFn = null; // Function to call when timer expires
    static _frequency = 0.5; // How often to check on timer, in seconds
    _callbackActive = false; // Used to ensure that only one callback
                             // is active at a time. 

    //**********************************************************
    // Duration in seconds, and function to execute
    constructor(secs, func)
    {
        duration = secs*1000;
        actionFn = func;
    }

    //**********************************************************
    // Start the timer
    function enable() 
    {
        enabled = true;
        startTime = hardware.millis();
        if (!_callbackActive)
        {
            imp.wakeup(_frequency, _timerCallback.bindenv(this));
            _callbackActive = true;
        }
    }

    //**********************************************************
    // Stop the timer
    function disable() 
    {
        if(enabled)
        {
            enabled = false;
            startTime = null;
        }
    }

    //**********************************************************
    // Internal function to manage cancelation and expiration
    function _timerCallback()
    {
        _callbackActive = false;
        if (enabled)
        {
            local elapsedTime = hardware.millis() - startTime;
            //server.log(format("%d/%d to sleep", elapsedTime, duration));
            if (elapsedTime > duration)
            {
                enabled = false;
                startTime = null;
                actionFn();
            }
            else
            {
                imp.wakeup(_frequency, _timerCallback.bindenv(this));
                _callbackActive = true;
            }
        }
    }
}


//======================================================================
// Handles any I2C device
class I2cDevice
{
    i2cPort = null;
    i2cAddress = null;

    constructor(port, address)
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
            server.log(format("Error: invalid I2C port specified: %c", port));
        }

        // Use the fastest supported clock speed
        i2cPort.configure(CLOCK_SPEED_400_KHZ);

        // Takes a 7-bit I2C address
        i2cAddress = address << 1;
    }

    // Read a byte
    function read(register)
    {
        local data = i2cPort.read(i2cAddress, format("%c", register), 1);
        if(data == null)
        {
            server.log("Error: I2C read failure");
            // TODO: this should return null, right??? Do better handling.
            return -1;
        }

        return data[0];
    }

    // Write a byte
    function write(register, data)
    {
        i2cPort.write(i2cAddress, format("%c%c", register, data));
    }

    // Print all registers in a range
    function printI2cRegs(min, max)
    {
        for (local i=min; i<max+1; i++)
        {
            printRegister(i, label);
        }
    }

    // Print register contents in hex and binary
    function printRegister(regIdx, label="")
    {
        local reg = read(regIdx);
        local regStr = "";
        for(local j=7; j>=0; j--)
        {
            if (reg & 1<<j) 
            {
                regStr += "1";
            }
            else
            {
                regStr += "0";
            }

            if (j==4) 
            {
                regStr += " ";
            }
        }

        server.log(format("REG 0x%02X=%s (0x%02X) %s", regIdx, regStr, 
                          reg, label));
    }
}


//======================================================================
// Handles the SX1508 GPIO expander
class IoExpanderDevice extends I2cDevice
{
    i2cPort = null;
    i2cAddress = null;
    irqCallbacks = array(8); // TODO move out? Shared by all instances?

    constructor(port, address)
    {
        base.constructor(port, address);

        // Disable "Autoclear NINT on RegData read". This 
        // could cause us to lose accelerometer interrupts
        // if someone reads or writes any pin between when 
        // an interrupt occurs and we handle it. 
        write(0x10, 0x01); // RegMisc

        // Lower the output buffer drive, to reduce current consumption
        write(0x02, 0xFF); // RegLowDrive

        // TODO Move this, as it would get called for each 
        // of multiple IOExpanders.  Or design it in better?
        if (!gPin1HandlerSet) {
            //server.log("--------Setting interrupt handler for pin1--------");
            hardware.pin1.configure(DIGITAL_IN_WAKEUP, 
                                    handlePin1Int.bindenv(this));
            gPin1HandlerSet = true;
        }
    }

    // Write a bit to a register
    function writeBit(register, bitn, level)
    {
        local value = read(register);
        value = (level == 0)?(value & ~(1<<bitn)):(value | (1<<bitn));
        write(register, value);
    }

    // Write a masked bit pattern
    function writeMasked(register, data, mask)
    {
        local value = read(register);
        value = (value & ~mask) | (data & mask);
        write(register, value);
    }

    // Get a GPIO input pin level
    function getPin(gpio)
    {
        return (read(0x08)&(1<<(gpio&7)))?1:0;
    }

    // Set a GPIO level
    function setPin(gpio, level)
    {
        writeBit(0x08, gpio&7, level?1:0);
    }

    // Set a GPIO direction
    function setDir(gpio, input)
    {
        writeBit(0x07, gpio&7, input?1:0);
    }

    // Set a GPIO internal pull up
    function setPullUp(gpio, enable)
    {
        writeBit(0x03, gpio&7, enable);
    }

    // Set a GPIO internal pull down
    function setPullDown(gpio, enable)
    {
        writeBit(0x04, gpio&7, enable);
    }

    // Set GPIO interrupt mask
    // "0" means disable interrupt, "1" means enable (opposite of datasheet)
    function setIrqMask(gpio, enable)
    {
        writeBit(0x09, gpio&7, enable?0:1); 
    }

    // Set GPIO interrupt edges
    function setIrqEdges(gpio, rising, falling)
    {
        local addr = 0x0B - (gpio>>2);
        local mask = 0x03 << ((gpio&3)<<1);
        local data = (2*falling + rising) << ((gpio&3)<<1);
        writeMasked(addr, data, mask);
    }

    // Clear all interrupts.  Must do this immediately after
    // reading the interrupt register in the handler, otherwise
    // we may get other interrupts in between and miss them. 
    function clearAllIrqs()
    {
        write(0x0C, 0xFF); // RegInterruptSource
    }

    // Set an interrupt handler callback for a particular pin
    function setIrqCallback(pin, func)
    {
        irqCallbacks[pin] = func;
    }

    // Handle all expander callbacks
    // TODO: if you have multiple IoExpanderDevice's, which instance is called?
    function handlePin1Int()
    {
        local regInterruptSource = 0;
        local reg = 0;

        // Get the active interrupt sources
        // Keep reading the interrupt source register and clearing 
        // interrupts until it reads clean.  This catches any interrupts
        // that occur between reading and clearing. 
        while (reg = read(0x0C)) // RegInterruptSource
        {
            clearAllIrqs();
            regInterruptSource = regInterruptSource | reg;
        }

        // If no interrupts, just return. This occurs on every 
        // pin 1 falling edge. 
        if (!regInterruptSource) 
        {
            return;
        }

        //printRegister(0x0C, "INTERRUPT");

        // Call the interrupt handlers for all active interrupts
        for(local pin=0; pin < 8; pin++){
            if(regInterruptSource & 1<<pin){
                //server.log(format("-Calling irq callback for pin %d", pin));
                irqCallbacks[pin]();
            }
        }
    }
    
    // Print and label expander registers
    function printI2cRegs(min=0, max=0x10)
    {
        local label = "";
        for (local i=min; i<max+1; i++)
        {
            switch (i) {
                case 0x03:
                    label = "RegPullUp";
                    break;
                case 0x04:
                    label = "RegPullDown";
                    break;
                case 0x07:
                    label = "RegDir";
                    break;
                case 0x08:
                    label = "RegData";
                    break;
                case 0x09:
                    label = "RegInterruptMask";
                    break;
                case 0x0A:
                    label = "RegSenseHigh";
                    break;
                case 0x0B:
                    label = "RegSenseLow";
                    break;
                case 0x0C:
                    label = "RegInterruptSource";
                    break;
                case 0x0D:
                    label = "RegEventStatus";
                    break;
                case 0x10:
                    label = "RegMisc";
                    break;
                default:
                    label = "";
                    break;
            }
            printRegister(i, label);
        }
    }
}


//======================================================================
// Device state machine 

//**********************************************************************
function updateDeviceState(newState)
{
    // Update the state 
    local oldState = gDeviceState;
    gDeviceState = newState;

    // If we are transitioning to idle, start the sleep timer. 
    // If transitioning out of idle, clear it.
    if (newState == DeviceState.IDLE)
    {
        if (oldState != DeviceState.IDLE)
        {
            // Start deep sleep timer
            gDeepSleepTimer.enable();
        }
    }
    else
    {
        // Disable deep sleep timer
        gDeepSleepTimer.disable();
    }

    // If we are transitioning to SCAN_RECORD, start the button timer. 
    // If transitioning out of SCAN_RECORD, clear it. The reason 
    // we don't time the actual button press is that, if we have 
    // captured a scan, we don't want to abort.
    if (newState == DeviceState.SCAN_RECORD)
    {
        if (oldState != DeviceState.SCAN_RECORD)
        {
            // Start timing button press
            gButtonTimer.enable();
        }
    }
    else
    {
        // Stop timing button press
        gButtonTimer.disable();
    }

    // Log the state change, for debugging
    //local os = (oldState==null) ? "null" : oldState.tostring();
    //local ns = (newState==null) ? "null" : newState.tostring();
    //server.log(format("State change: %s -> %s", os, ns));

    // Verify state machine is in order 
    switch (newState) 
    {
        case DeviceState.IDLE:
            assert(oldState == DeviceState.BUTTON_RELEASED ||
                   oldState == null);
            break;
        case DeviceState.SCAN_RECORD:
            assert(oldState == DeviceState.IDLE);
            break;
        case DeviceState.SCAN_CAPTURED:
            assert(oldState == DeviceState.SCAN_RECORD);
            break;
        case DeviceState.BUTTON_TIMEOUT:
            assert(oldState == DeviceState.SCAN_RECORD);
            break;
        case DeviceState.BUTTON_RELEASED:
            assert(oldState == DeviceState.SCAN_RECORD ||
                   oldState == DeviceState.SCAN_CAPTURED ||
                   oldState == DeviceState.BUTTON_TIMEOUT);
            break;
        default:
            assert(false);
            break;
    }
}


//======================================================================
// Scanner
class Scanner extends IoExpanderDevice
{
    pin = null; // IO expander pin assignment (trigger)
    reset = null; // IO expander pin assignment (reset)
    scannerOutput = "";  // Stores the current barcode characters

    constructor(port, address, triggerPin, resetPin)
    {
        base.constructor(port, address);

        // Save assignments
        pin = triggerPin;
        reset = resetPin;

        // Reset the scanner at each boot, just to be safe
        setDir(reset, 0); // set as output
        setPullUp(reset, 0); // disable pullup
        setPin(reset, 0); // pull low to reset
        imp.sleep(0.001); // wait for x seconds
        setPin(reset, 1); // pull high to boot

        // Configure trigger pin as output
        setDir(pin, 0); // set as output
        setPullUp(pin, 0); // disable pullup
        setPin(pin, 1); // pull high to disable trigger

        // Configure scanner UART (for RX only)
        hardware.configure(UART_57); 
        hardware.uart57.configure(38400, 8, PARITY_NONE, 1, NO_CTSRTS | NO_TX, 
                                 scannerCallback.bindenv(this));
    }

    // Disable for low power sleep mode
    function disable()
    {
        setPin(reset, 0); // pull reset low 
        setPin(pin, 0); // pull trigger low 
        hardware.uart57.disable();
    }

    function readTriggerState()
    {
        return getPin(pin);
    }

    function trigger(on)
    {
        if (on)
        {
            setPin(pin, 0);
        }
        else
        {
            setPin(pin, 1);
        }
    }

    //**********************************************************************
    // Start the scanner and sampler
    function startScanRecord() 
    {
        // Trigger the scanner
        hwScanner.trigger(true);

        // Trigger the mic recording
        gAudioBufferOverran = false;
        gAudioChunkCount = 0;
        gLastSamplerBuffer = null; 
        gLastSamplerBufLen = 0; 
        agent.send("startAudioUpload", "");
        hardware.sampler.start();
    }

    //**********************************************************************
    // Stop the scanner and sampler
    // Note: this function may be called multiple times in a row, so
    // it must support that. 
    function stopScanRecord()
    {
        // Stop mic recording
        hardware.sampler.stop();

        // Release scanner trigger
        hwScanner.trigger(false);

        // Reset for next scan
        scannerOutput = "";
    }

    //**********************************************************************
    // Scanner data ready callback, called whenever there is data from scanner.
    // Reads the bytes, and detects and handles a full barcode string.
    function scannerCallback()
    {
        // Read the first byte
        local data = hardware.uart57.read();
        while (data != -1)  
        {
            //server.log("char " + data + " \"" + data.tochar() + "\"");

            // Handle the data
            switch (data) 
            {
                case '\n':
                    // Scan complete. Discard the line ending,
                    // upload the beep, and reset state.

                    // If the scan came in late (e.g. after button up), 
                    // discard it, to maintain the state machine. 
                    if (gDeviceState != DeviceState.SCAN_RECORD)
                    {
                        //server.log(format(
                                   //"Got capture too late. Dropping scan %d",
                                   //gDeviceState));
                        scannerOutput = "";
                        return;
                    }
                    updateDeviceState(DeviceState.SCAN_CAPTURED);

                    //server.log("Code: \"" + scannerOutput + "\" (" + 
                               //scannerOutput.len() + " chars)");
                    agent.send("uploadBeep", {
                                              scandata=scannerOutput,
                                              scansize=scannerOutput.len(),
                                              serial=hardware.getimpeeid(),
                                              fw_version=cFirmwareVersion,
                                              linkedrecord="",
                                              audiodata="",
                                             });
                    
                    // Stop collecting data
                    stopScanRecord();
                    break;

                case '\r':
                    // Discard line endings
                    break;

                default:
                    // Store the character
                    scannerOutput = scannerOutput + data.tochar();
                    break;
            }

            // Read the next byte
            data = hardware.uart57.read();
        } 

        //server.log("EXITING CALLBACK");
    }
}


//======================================================================
// Button
enum ButtonState
{
    BUTTON_UP,
    BUTTON_DOWN,
}

class PushButton extends IoExpanderDevice
{
    pin = null; // IO expander pin assignment
    buttonState = ButtonState.BUTTON_UP; // Button current state

    // WARNING: increasing these can cause buffer overruns during 
    // audio recording, because this the button debouncing on "up"
    // happens before the audio sampler buffer is serviced. 
    static numSamples = 5; // For debouncing
    static sleepSecs = 0.004;  // For debouncing

    constructor(port, address, btnPin)
    {
        base.constructor(port, address);

        // Save assignments
        pin = btnPin;

        // Set event handler for IRQ
        setIrqCallback(btnPin, buttonCallback.bindenv(this));

        // Configure pin as input, IRQ on both edges
        setDir(pin, 1); // set as input
        setPullUp(pin, 1); // enable pullup
        setIrqMask(pin, 1); // enable IRQ
        setIrqEdges(pin, 1, 1); // rising and falling
    }

    function readState()
    {
        return getPin(pin);
    }

    //**********************************************************************
    // If we are gathering data and the button has been held down 
    // too long, we abort recording and scanning.
    function handleButtonTimeout()
    {
        updateDeviceState(DeviceState.BUTTON_TIMEOUT);
        hwScanner.stopScanRecord();
        hwPiezo.playSound("timeout");
        server.log("Timeout reached. Aborting scan and record.");
    }

    //**********************************************************************
    // Button handler callback 
    // Not a true interrupt handler, this cannot interrupt other Squirrel 
    // code. The event is queued and the callback is called next time the 
    // Imp is idle.
    function buttonCallback()
    {
        // Sample the button multiple times to debounce. Total time 
        // taken is (numSamples-1)*sleepSecs
        local state = readState()
        for (local i=1; i<numSamples; i++)
        {
            state += readState()
            imp.sleep(sleepSecs)
        }

        // Handle the button state transition
        switch(state) 
        {
            case 0:
                // Button in held state
                if (buttonState == ButtonState.BUTTON_UP)
                {
                    updateDeviceState(DeviceState.SCAN_RECORD);
                    buttonState = ButtonState.BUTTON_DOWN;
                    server.log("Button state change: DOWN");

                    hwScanner.startScanRecord();
                }
                break;
            case numSamples:
                // Button in released state
                if (buttonState == ButtonState.BUTTON_DOWN)
                {
                    buttonState = ButtonState.BUTTON_UP;
                    server.log("Button state change: UP");

                    local oldState = gDeviceState;
                    updateDeviceState(DeviceState.BUTTON_RELEASED);

                    if (oldState == DeviceState.SCAN_RECORD)
                    {
                        // Audio captured. Stop sampling and send it. 
                        // Note that we only call sendLastBuffer in
                        // the case that we want to capture the audio, 
                        // so it cannot be inside stopScanRecord, which 
                        // is called in multiple places. 
                        // We have two uses of imp.onidle(), one during 
                        // the IDLE state and one when not idle.  They 
                        // must be kept separate, as only one onidle 
                        // callback is supported at a time. 
                        gSamplerStopping = true;
                        imp.onidle(sendLastBuffer); 
                        hwScanner.stopScanRecord();
                    }
                    // No more work to do, so go to idle
                    updateDeviceState(DeviceState.IDLE);
                }
                break;
            default:
                // Button is in transition (not settled)
                //server.log("Bouncing! " + buttonState);
                break;
        }
    }
}


//======================================================================
// Charge status pin
/* TODO MEMORY USAGE NOTES: 
  Before adding charger IoExpander class: 24400 bytes 
  After adding class: 24200 to 24224 bytes (200 delta)
  After making ChargeStatus subclass: 20904 to 20928 bytes (3,496 delta)

  If wait 1 second:
    Before: 27708 bytes
    After ChargeStatus: 24940 bytes (delta of 2,768 bytes)

  Basically, we can recover up to 2.8kB for each IoExpander subclass 
  by eliminating them. 
*/
class ChargeStatus extends IoExpanderDevice
{
    pin = null; // IO expander pin assignment

    constructor(port, address, chargePin)
    {
        base.constructor(port, address);

        // Save assignments
        pin = chargePin;

        // Set event handler for IRQ
        setIrqCallback(pin, chargerCallback.bindenv(this));

        // Configure pin as input, IRQ on both edges
        setDir(pin, 1); // set as input
        setPullUp(pin, 1); // enable pullup
        setIrqMask(pin, 1); // enable IRQ
        setIrqEdges(pin, 1, 1); // rising and falling
    }

    function readState()
    {
        return getPin(pin);
    }

    function isCharging()
    {
        if(getPin(pin))
        {
            return false;
        }
        return true;
    }

    //**********************************************************************
    // Charge status interrupt handler callback 
    function chargerCallback()
    {
        if (isCharging()) 
        {
            server.log("Charger plugged in");
        }
        else
        {
            server.log("Charger unplugged");
        }
    }
}


//======================================================================
// 3.3 volt switch pin for powering most peripherals
class Switch3v3Accessory extends IoExpanderDevice
{
    pin = null; // IO expander pin assignment

    constructor(port, address, switchPin)
    {
        base.constructor(port, address);

        // Save assignments
        pin = switchPin;

        // Configure pin 
        setDir(pin, 0); // set as output
        setPullUp(pin, 0); // disable pullup
        setPin(pin, 0); // pull low to turn switch on
    }

    function readState()
    {
        return getPin(pin);
    }

    function enable()
    {
        setPin(pin, 0);
    }

    function disable()
    {
        setPin(pin, 1);
    }
}


//======================================================================
// Accelerometer

// Interrupt pin for accelerometer I2C device
class AccelerometerInt extends IoExpanderDevice
{
    pin = null; // IO expander pin assignment
    accelIntHandler = null; // Our interrupt handler, from parent

    constructor(port, address, accelPin, handler)
    {
        base.constructor(port, address);

        // Save assignments
        pin = accelPin;
        accelIntHandler = handler;

        // Configure pin as input, IRQ on both edges
        setDir(pin, 1); // set as input
        setPullDown(pin, 1); // enable pulldown
        setIrqMask(pin, 1); // enable IRQ
        setIrqEdges(pin, 1, 0); // rising only

        // Set event handler for IRQ
        setIrqCallback(pin, accelIntHandler);
    }

    function readState()
    {
        return getPin(pin);
    }
}


// Accelerometer I2C device
class Accelerometer extends I2cDevice
{
    i2cPort = null;
    i2cAddress = null;
    interruptDevice = null; 
    reenableInterrupts = false;  // Allow interrupts to be re-enabled after 
                                 // an interrupt

    constructor(port, address, pin, expanderAddress)
    {
        base.constructor(port, address);

        // Verify communication by reading WHO_AM_I register
        local whoami = read(0x0F);
        if (whoami != 0x33)
        {
            server.log(format("Error reading accelerometer; whoami=0x%02X", 
                              whoami));
        }

        // Configure and enable accelerometer and interrupt
        write(0x20, 0x2F); // CTRL_REG1: 10 Hz, low power mode, 
                             // all 3 axes enabled

        write(0x21, 0x09); // CTRL_REG2: Enable high pass filtering and data

        enableInterrupts();
        //disableInterrupts();

        write(0x23, 0x00); // CTRL_REG4: Default related control settings

        write(0x24, 0x08); // CTRL_REG5: Interrupt latched

        // Note: maximum value is 0111 11111 (0x7F). High bit must be 0.
        write(0x32, 0x10); // INT1_THS: Threshold

        write(0x33, 0x00); // INT1_DURATION: any duration

        // Read HP_FILTER_RESET register to set filter. See app note 
        // section 6.3.3. It sounds like this might be the REFERENCE
        // register, 0x26. Commented out as I found it is not needed. 
        //read(0x26);

        write(0x30, 0x2A); // INT1_CFG: Enable OR interrupt for 
                           // "high" values of X, Y, Z

        // Clear interrupts before setting handler.  This is needed 
        // otherwise we get a spurious interrupt at boot. 
        clearAccelInterrupt();

        // Set the interrupt handler 
        interruptDevice = AccelerometerInt(port, expanderAddress, 
                pin, handleAccelInt.bindenv(this));
    }

    function enableInterrupts()
    {
        write(0x22, 0x40); // CTRL_REG3: Enable AOI interrupts
    }

    function disableInterrupts()
    {
        write(0x22, 0x00); // CTRL_REG3: Disable AOI interrupts
    }

    function clearAccelInterruptUntilCleared()
    {
        // Repeatedly clear the accel interrupt by reading INT1_SRC
        // until there are no interrupts left
        // WARNING: adding server.log statements in this function
        // causes it to fail for some reason
        local reg;
        while ((reg = read(0x31)) != 0x15)
        {
            imp.sleep(0.001);
        }
    }

    function clearAccelInterrupt()
    {
        read(0x31); // Clear the accel interrupt by reading INT1_SRC
    }

    function handleAccelInt() 
    {
        disableInterrupts();
        clearAccelInterrupt();
        if(reenableInterrupts)
        {
            enableInterrupts();
        }
    }

    // Debug printout of the orientation registers on one line
    function printOrientation() 
    {
        local values = [];
        local reg;

        // Read the X, Y, and Z acceleration values
        for(local i = 0x28; i < 0x2E; i++)
        {
            reg = read(i);
            
            // Convert from 2's complement)
            if (reg & 0x80)
            {
                reg = -((~(reg-1))& 0x7F);
            }

            values.append(reg);
        }

        // Log the results
        //printRegister(0x31, "INT1_SRC");
        printRegister(0x32, "INT1_THS");
        server.log(format(
                    "XH=%03d, YH=%03d, ZH=%03d, XL=%03d, YL=%03d, ZL=%03d", 
                          values[1], values[3], values[5],
                          values[0], values[2], values[4]
                         ));
    }

    // Print and label expander registers
    function printI2cRegs(min=0x07, max=0x3D)
    {
        local label = "";
        for (local i=min; i<max+1; i++)
        {
            // Skip reserved registers
            if ((i>=0x10 && i<=0x1E) || (i>=0x34 && i<=0x37)) 
            {
                continue;
            }
            // Stop early because the rest aren't that important
            if (i>0x33) 
            {
                continue;
            }

            switch (i) {
                case 0x0E:
                    label = "INT_COUNTER_REG";
                    break;
                case 0x0F:
                    label = "WHO_AM_I (should be 0x33)";
                    break;
                case 0x20:
                    label = "CTRL_REG1";
                    break;
                case 0x21:
                    label = "CTRL_REG2";
                    break;
                case 0x22:
                    label = "CTRL_REG3";
                    break;
                case 0x23:
                    label = "CTRL_REG4";
                    break;
                case 0x24:
                    label = "CTRL_REG5";
                    break;
                case 0x25:
                    label = "CTRL_REG6";
                    break;
                case 0x26:
                    label = "REFERENCE";
                    break;
                case 0x27:
                    label = "STATUS_REG2";
                    break;
                case 0x28:
                    label = "OUT_X_L";
                    break;
                case 0x29:
                    label = "OUT_X_H";
                    break;
                case 0x2A:
                    label = "OUT_Y_L";
                    break;
                case 0x2B:
                    label = "OUT_Y_H";
                    break;
                case 0x2C:
                    label = "OUT_Z_L";
                    break;
                case 0x2D:
                    label = "OUT_Z_H";
                    break;
                case 0x30:
                    label = "INT1_CFG";
                    break;
                case 0x31:
                    label = "INT1_SOURCE";
                    break;
                case 0x32:
                    label = "INT1_THS";
                    break;
                case 0x33:
                    label = "INT1_DURATION";
                    break;
                default:
                    label = "";
                    break;
            }
            printRegister(i, label);
        }
    }
}


//======================================================================
// Sampler/Audio In

//**********************************************************************
// Agent callback: upload complete
agent.on("uploadCompleted", function(result) {
    hwPiezo.playSound(result);
});


//**********************************************************************
// Process the last buffer, if any, and tell the agent we are done. 
// This function is called after sampler.stop in a way that 
// ensures we have captured all sampled buffers. 
function sendLastBuffer()
{
    // Send the last chunk to the server, if there is one
    if (gLastSamplerBuffer != null && gLastSamplerBufLen > 0)
    {
        agent.send("uploadAudioChunk", {buffer=gLastSamplerBuffer, 
                   length=gLastSamplerBufLen});
    }

    // If there are less than x secs of audio, abandon the 
    // recording. Else send the beep!
    local secs = gAudioChunkCount*gAudioBufferSize/
                       gAudioSampleRate.tofloat();

    //Because we cannot guarantee network robustness, we allow 
    // uploads even if an overrun occurred. Worst case it still
    // fails to reco, and you'll get an equivalent error. 
    //if (secs >= 0.4 && !gAudioBufferOverran)
    if (secs >= 0.4)
    {
        agent.send("endAudioUpload", {
                                      scandata="",
                                      serial=hardware.getimpeeid(),
                                      fw_version=cFirmwareVersion,
                                      linkedrecord="",
                                      audiodata="", // to be added by agent
                                      scansize=gAudioChunkCount, 
                                     });
    }

    // We have completed the process of stopping the sampler
    gSamplerStopping = false;
}


//**********************************************************************
// Called when an audio sampler buffer is ready.  It is called 
// ((sample rate * bytes per sample)/buffer size) times per second.  
// So for 16 kHz sampling of 8-bit A law and 2000 byte buffers, 
// it is called 8x/sec. 
// 
// Since A-law seems to only send full buffers, we send the whole 
// buffer and truncate if necessary on the server side, instead 
// of making a (possibly truncated) copy each time here. This 
// is filed as a bug that may be fixed in the future. 
// 
// Buffer overruns can be caused (and typically are) by this routine
// taking too long.  It typically takes too long if the network is slow
// or flakey when we upload samples.  We are robust if this callback
// takes up to about 100ms.  Typically it should take about 3ms.  
function samplerCallback(buffer, length)
{
    //server.log("SAMPLER CALLBACK: size " + length");
    if (length <= 0)
    {
        gAudioBufferOverran = true;
        server.log("Error: audio sampler buffer overrun!!!!!!, last timer="+gAudioTimer+"ms");
    }
    else 
    {
        // Time the sending
        gAudioChunkCount++;
        gAudioTimer = hardware.millis();

        // Send the data, managing the last buffer as a special case
        if (gSamplerStopping) {
            if (gLastSamplerBuffer) { 
                // It wasn't quite the last one, send normally
                agent.send("uploadAudioChunk", {buffer=buffer, 
                           length=length});
            }
            // Process last buffer later, to do special handling
            gLastSamplerBuffer = buffer;
            gLastSamplerBufLen = length;
        }
        else
        {
            agent.send("uploadAudioChunk", {buffer=buffer, length=length});
        }

        // Finish timing the send.  See function comments for more info. 
        gAudioTimer = hardware.millis() - gAudioTimer;
        //server.log(gAudioTimer + "ms");
    }
}


//======================================================================
// Utilities

//**********************************************************************

//**********************************************************************
// Temporary function to catch dumb mistakes
function print(str)
{
    server.log("ERROR USED PRINT FUNCTION. USE SERVER.LOG INSTEAD.");
}


//**********************************************************************
function printStartupDebugInfo()
{
    // impee ID
    server.log(format("Your impee ID: %s", hardware.getimpeeid()));

    // free memory
    server.log(format("Free memory: %d bytes", imp.getmemoryfree()));

    // hardware voltage
    server.log(format("Power supply voltage: %.2fv", hardware.voltage()));

    // Wi-Fi signal strength
    local bars = 0;
    local rssi = imp.rssi();
    if (rssi == 0)
        bars = -1;
    else if (rssi > -67)
        bars = 5;
    else if (rssi > -72)
        bars = 4;
    else if (rssi > -77)
        bars = 3; 
    else if (rssi > -82)
        bars = 2;
    else if (rssi > -87)
        bars = 1; 

    if (bars == -1)
    {
        server.log("Wi-Fi not connected");
    }
    else
    {
        server.log(format("Wi-Fi signal: %d bars (%d dBm)", bars, rssi));
    }

    // Wi-Fi base station ID
    server.log(format("Wi-Fi SSID: %s", imp.getbssid()));

    // Wi-Fi MAC address
    server.log(format("Wi-Fi MAC: %s", imp.getmacaddress()));

    // Charge status
    server.log(format("Device is %scharging", 
                      chargeStatus.isCharging()?"":"not \x00"));
}


//**********************************************************************
function init()
{
    imp.configure("hiku", [], []);

    // We will always be in deep sleep unless button pressed, in which
    // case we need to be as responsive as possible. 
    imp.setpowersave(false);

    // I2C bus addresses
    const cAddrIoExpander = 0x23;
    const cAddrAccelerometer = 0x18;

    // IO expander pin assignments
    const cIoPinAccelerometerInt = 0;
    const cIoPinChargeStatus = 1;
    const cIoPinButton =  2;
    const cIoPin3v3Switch =  4;
    const cIoPinScannerTrigger =  5;
    const cIoPinScannerReset =  6;

    // 3v3 accessory switch config
    sw3v3 <- Switch3v3Accessory(I2C_89, cAddrIoExpander, cIoPin3v3Switch);
    sw3v3.enable();
 
    // Charge status detect config
    chargeStatus <- ChargeStatus(I2C_89, cAddrIoExpander, cIoPinChargeStatus);

    // Piezo config
    hwPiezo <- Piezo(hardware.pin5);

    // Button config
    hwButton <- PushButton(I2C_89, cAddrIoExpander, cIoPinButton);

    // Scanner config
    hwScanner <-Scanner(I2C_89, cAddrIoExpander, cIoPinScannerTrigger,
                        cIoPinScannerReset);

    // Microphone sampler config
    hwMicrophone <- hardware.pin2;
    hardware.sampler.configure(hwMicrophone, gAudioSampleRate, 
                               [buf1, buf2, buf3], 
                               samplerCallback, NORMALISE | A_LAW_COMPRESS); 

    // Accelerometer config
    hwAccelerometer <- Accelerometer(I2C_89, cAddrAccelerometer, 
                                     cIoPinAccelerometerInt, cAddrIoExpander);

    // Create our timers
    gButtonTimer <- CancellableTimer(cButtonTimeout, 
                                     hwButton.handleButtonTimeout.bindenv(
                                         hwButton)
                                    );
    gDeepSleepTimer <- CancellableTimer(cDelayBeforeDeepSleep, sleepHandler);

    // Send the agent our impee ID
    agent.send("impeeId", hardware.getimpeeid());

    // Transition to the idle state
    updateDeviceState(DeviceState.IDLE);

    // Print debug info
    // WARNING: for some reason, if this is uncommented, the device
    // will not wake up if there is motion while the device goes 
    // to sleep!
    //printStartupDebugInfo();

    // Initialization complete notification
    hwPiezo.playSound("startup"); 
}


//**********************************************************************
// Do pre-sleep configuration and initiate deep sleep
function sleepHandler() {
    // Disable the scanner and its UART
    hwScanner.disable();

    // Disable the SW 3.3v switch, to save power during deep sleep
    sw3v3.disable();

    // Re-enable accelerometer interrupts
    hwAccelerometer.reenableInterrupts = true;
    hwAccelerometer.enableInterrupts();

    // Handle any last interrupts before we clear them all and go to sleep
    hwButton.handlePin1Int(); 

    // Clear any accelerometer interrupts, then clear the IO expander. 
    // We found this to be necessary to not hang on sleep, as we were
    // getting spurious interrupts from the accelerometer when re-enabling,
    // that were not caught by handlePin1Int. Race condition? 
    hwAccelerometer.clearAccelInterruptUntilCleared();
    hwButton.clearAllIrqs(); 

    // Force the imp to sleep immediately, to avoid catching more interrupts
    server.sleepfor(cDeepSleepDuration);
}


//**********************************************************************
// main
init();

// We only wake due to an interrupt or after power loss.  If the 
// former, we need to handle any pending interrupts. 
hwButton.handlePin1Int();

