// Copyright 2013 Katmandu Technology, Inc. All rights reserved. Confidential.


// Consts and enums
const cButtonTimeout = 6000;  // in milliseconds
const cDelayBeforeDeepSleep = 10.0;  // in seconds
const cDeepSleepDuration = 120.0;  // in seconds

enum ButtonState
{
    BUTTON_UP,
    BUTTON_DOWN,
}

enum DeviceState
/*
                           ---> SCAN_CAPTURED ------>
                          /                          \
    IDLE ---> SCAN_RECORD ---> BUTTON_TIMEOUT -->     \
                          \                      \     \
                           -------------------> BUTTON_RELEASED ---> IDLE
*/
{
    IDLE,             // Not recording or processing data
    SCAN_RECORD,      // Scanning and recording audio
    SCAN_CAPTURED,    // Processing scan data
    BUTTON_TIMEOUT,   // Timeout limit reached while holding button
    BUTTON_RELEASED,  // Button released, may have audio to send
}


// Globals
// TODO: reduce frequency and scope of global variable access
gDeviceState <- null; // Hiku device current state
gButtonState <- ButtonState.BUTTON_UP; // Button current state

gScannerOutput <- ""; // String containing barcode data
gAudioBufferOverran <- false; // True if an overrun occurred
gTimeButtonPressed <- null; // Used for held-too-long timeout
gDeepSleepTimer <- null; // Handles deep sleep timeouts

// If we use fewer than four or smaller than 6000 byte buffers, we 
// get buffer overruns during scanner RX. Even with this 
// setup, we still (rarely) get an overrun.  We may not care, as we 
// currently throw away audio if we get a successful scan. 
// TODO: can we increase the scanner UART baud rate to improve this? 
// TODO: A-law sampler does not return partial buffers. This means that up to 
// the last buffer size of data is dropped. What size is optimal?
buf1 <- blob(6000);
buf2 <- blob(6000);
buf3 <- blob(6000);
buf4 <- blob(6000);


//**********************************************************************
// Timer that can be canceled and executes a function when expiring
// 
// Example usage: set up a timer to call dosomething() in 10 minutes
//   doSomethingTimer = ActionTimer(60*10, function(){dosomething();});
//   doSomethingTimer.enable()
//   doSomethingTimer.disable()
class ActionTimer
{
    enabled = null;
    startTime = null; // Timer starting time, in milliseconds
    duration = null; // How long the timer should run before doing the action
    actionFn = null; // Function to call when timer expires
    static _frequency = 0.5; // How often to check on timer, in seconds
    _callbackActive = false; // Used to ensure that only one callback
                             // is active at a time. 

    // Duration in seconds, and function to execute
    constructor(secs, func)
    {
        duration = secs*1000;
        actionFn = func;
    }

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

    // Stop the timer
    function disable() 
    {
        if(enabled)
        {
            enabled = false;
            startTime = null;
        }
    }

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


//**********************************************************************
// Temporary function to catch dumb mistakes
function print(str)
{
    server.log("ERROR USED PRINT FUNCTION. USE SERVER.LOG INSTEAD.");
}


//**********************************************************************
// Agent callback: upload complete
agent.on(("uploadCompleted"), function(msg) {
    //server.log(format("in agent.on uploadCompleted, msg=%s", msg));
    beep(msg);
});


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

    // TODO: assert to verify state machine is in order 
}


//**********************************************************************
// If we are gathering data and the button has been held down 
// too long, we abort recording and scanning.
function buttonTimerCallback()
{
    if (gDeviceState == DeviceState.SCAN_RECORD)
    {
        // In theory millis can wrap, but it is reset when we sleep
        local elapsedTime = hardware.millis() - gTimeButtonPressed;
        //server.log(format("in buttonTimerCallback, time = %d", elapsedTime));
        
        if (elapsedTime > cButtonTimeout)
        {
            // We have timed out.  Update state and give an error. 
            updateDeviceState(DeviceState.BUTTON_TIMEOUT);
            stopScanRecord();
            beep("failure");
            server.log("Timeout reached. Aborting scan and record.");
        }
    }

    imp.wakeup(0.5, buttonTimerCallback);
}


//**********************************************************************
//TODO: review and cleanup
//TODO: instead of reconfiguring, just change duty cycle to zero via
//      pin.write(0). Does this have power impact? 
//TODO: minimize impact of busy wait -- slows responsiveness. Can do 
//      async and maintain sound?
function beep(tone = "success") 
{
    switch (tone) 
    {
        case "success":
            hwPiezo.configure(PWM_OUT, 0.0015, 0.5);
            imp.sleep(0.2);
            hwPiezo.configure(PWM_OUT, 0.00075, 0.5);
            imp.sleep(0.2);
            hwPiezo.configure(DIGITAL_OUT);
            hwPiezo.write(0);
            break;
        case "failure":
            hwPiezo.configure(PWM_OUT, 0.00075, 0.5);
            imp.sleep(0.2);
            hwPiezo.configure(PWM_OUT, 0.0015, 0.5);
            imp.sleep(0.2);
            hwPiezo.configure(DIGITAL_OUT);
            hwPiezo.write(0);
            break;
        case "sleep":
            hwPiezo.configure(PWM_OUT, 0.0015, 0.5);
            imp.sleep(0.2);
            hwPiezo.configure(PWM_OUT, 0.0020, 0.5);
            imp.sleep(0.2);
            hwPiezo.configure(DIGITAL_OUT);
            hwPiezo.write(0);
            break;
        case "startup":
            hwPiezo.configure(PWM_OUT, 0.0015, 0.5);
            imp.sleep(0.2);
            hwPiezo.configure(PWM_OUT, 0.0015, 0.5);
            imp.sleep(0.2);
            hwPiezo.configure(PWM_OUT, 0.00075, 0.5);
            imp.sleep(0.2);
            hwPiezo.configure(DIGITAL_OUT);
            hwPiezo.write(0);
            break;
        default:
            server.log(format("Unknown beep tone requested: \"%s\"", msg));
            break;
    }
}


//**********************************************************************
// Stop the scanner and sampler
function stopScanRecord()
{
    // Release scanner trigger
    hwTrigger.write(1); 

    // Reset for next scan
    gScannerOutput = "";

    // Stop mic recording
    hardware.sampler.stop();
}


//**********************************************************************
// Scanner data ready callback, called whenever there is data from scanner.
// Reads the bytes, and detects and handles a full barcode string.
function scannerCallback()
{
    // Read the first byte
    local data = hwScannerUart.read();
    while (data != -1)  
    {
        //server.log("char " + data + " \"" + data.tochar() + "\"");

        // Handle the data
        switch (data) 
        {
            case '\n':
                // Scan complete. Discard the line ending,
                // upload the beep, and reset state.
                updateDeviceState(DeviceState.SCAN_CAPTURED);

                //server.log("Code: \"" + gScannerOutput + "\" (" + 
                           //gScannerOutput.len() + " chars)");
                agent.send("uploadBeep", {barcode=gScannerOutput});
                
                // Stop collecting data
                stopScanRecord();
                break;

            case '\r':
                // Discard line endings
                break;

            default:
                // Store the character
                gScannerOutput = gScannerOutput + data.tochar();
                break;
        }

        // Read the next byte
        data = hwScannerUart.read();
    } 

    //server.log("EXITING CALLBACK");
}


//**********************************************************************
// Button handler callback 
// Not a true interrupt handler, this cannot interrupt other Squirrel code. 
// The event is queued and the callback is called next time the Imp is idle. 
function buttonCallback()
{
    // Sample the button multiple times to debounce. Total time 
    // taken is (numSamples-1)*sleepSecs
    const numSamples = 4;
    const sleepSecs = 0.003; 
    local buttonState = hwButton.read()
    for (local i=1;i<numSamples;i++)
    {
        buttonState += hwButton.read()
        imp.sleep(sleepSecs)
    }

    // Handle the button state transition
    switch(buttonState) 
    {
        case numSamples:
            // Button in held state
            if (gButtonState == ButtonState.BUTTON_UP)
            {
                // TODO: move to updateDeviceState
                if (gDeviceState != DeviceState.IDLE)
                {
                    server.log(format("ERROR: expected IDLE, got state %d", 
                               gDeviceState));
                }

                updateDeviceState(DeviceState.SCAN_RECORD);
                gButtonState = ButtonState.BUTTON_DOWN;
                //server.log("Button state change: DOWN " + gButtonState);

                // Trigger the scanner
                hwTrigger.write(0);

                // Trigger the mic recording
                gAudioBufferOverran = false;
                agent.send("startAudioUpload", "");
                hardware.sampler.start();

                // Start timing button press
                gTimeButtonPressed = hardware.millis();
            }
            break;
        case 0:
            // Button in released state
            if (gButtonState == ButtonState.BUTTON_DOWN)
            {
                gButtonState = ButtonState.BUTTON_UP;
                //server.log("Button state change: UP " + gButtonState);

                switch (gDeviceState) 
                {
                    case DeviceState.BUTTON_TIMEOUT:
                    case DeviceState.SCAN_CAPTURED:
                        // Handling complete, so go straight to idle
                        updateDeviceState(DeviceState.IDLE);
                        break;

                    case DeviceState.SCAN_RECORD:
                        // Audio captured. Validate and send it
                        updateDeviceState(DeviceState.BUTTON_RELEASED);

                        // Stop collecting data
                        stopScanRecord();

                        // If we have good data, send it to the server
                        if (gAudioBufferOverran)
                        {
                            beep("failure");
                        }
                        else
                        {
                            agent.send("endAudioUpload", "");
                        }

                        // No more work to do, so go to idle
                        updateDeviceState(DeviceState.IDLE);
                        break;

                    default:
                        // IDLE, BUTTON_RELEASED, or unknown
                        server.log("ERROR: unexpected state %d", gDeviceState)
                        updateDeviceState(DeviceState.IDLE);
                        break;
                }
            }
            break;
        default:
            // Button is in transition (not settled)
            //server.log("Bouncing! " + buttonState);
            break;
    }
}


//**********************************************************************
// Called when an audio sampler buffer is ready.  It is called 
// ((sample rate * bytes per sample)/buffer size) times per second.  
// So for 16 kHz sampling of 8-bit A law and 2000 byte buffers, 
// it is called 8x/sec. 
function samplerCallback(buffer, length)
{
    //server.log("SAMPLER CALLBACK: size " + length");
    if (length <= 0)
    {
        gAudioBufferOverran = true;
        server.log("Audio sampler buffer overrun!!!!!!");
    }
    else 
    {
        // Output the buffer.  Since A-law seems to only send full
        // buffers, we send the whole buffer and truncate if necessary 
        // on the server side, instead of making a (possibly truncated) 
        // copy each time here. 
        //server.log("START uploading chunk");
        agent.send("uploadAudioChunk", {buffer=buffer, length=length});
        //server.log("END uploading chunk");
    }
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
}


//**********************************************************************
function init()
{
    imp.configure("hiku", [], [])

    // We will always be in deep sleep unless button pressed, in which
    // case we need to be as responsive as possible. 
    imp.setpowersave(false);

    // Pin assignment
    hwButton <-hardware.pin1;         // Move to IO expander
    hwMicrophone <- hardware.pin2;
    hwPiezo <- hardware.pin5;
    hwScannerUart <- hardware.uart57;
    hwTrigger <-hardware.pin8;        // Move to IO expander

    // Pin configuration
    hwButton.configure(DIGITAL_IN_WAKEUP, buttonCallback);
    hwTrigger.configure(DIGITAL_OUT);
    hwTrigger.write(1); // De-assert trigger by default
    hwPiezo.configure(DIGITAL_OUT);
    hwPiezo.write(0); // Turn off piezo by default

    // Microphone sampler config
    hardware.sampler.configure(hwMicrophone, 16000, [buf1, buf2, buf3, buf4], 
                               samplerCallback, NORMALISE | A_LAW_COMPRESS); 

    // Scanner UART config 
    hardware.configure(UART_57); 
    hwScannerUart.configure(9600, 8, PARITY_NONE, 1, NO_CTSRTS | NO_TX, 
                             scannerCallback);

    // Set up button timer callback, to detect long presses
    imp.wakeup(0.5, buttonTimerCallback);

    // Print debug info
    printStartupDebugInfo();

    // Initialization complete notification
    beep("startup"); 

    // Create our timers
    gDeepSleepTimer = ActionTimer(cDelayBeforeDeepSleep,
            function() {
                beep("sleep");
                server.log("going to sleep..."); 
                server.sleepfor(cDeepSleepDuration); 
            });

    // Transition to the idle state
    updateDeviceState(DeviceState.IDLE);

    // If we booted with the button held down, we are most likely
    // woken up by the button, so go directly to button handling.  
    // TODO: change when we get the IO expander.  In that case we need
    // to check the interrupt source. 
    buttonCallback();
}


//**********************************************************************
// main
init();

