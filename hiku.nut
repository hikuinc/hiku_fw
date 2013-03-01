// Copyright 2013 Katmandu Technology, Inc. All rights reserved. Confidential.


// Consts and enums
const cButtonTimeout = 6;  // in seconds
const cDelayBeforeDeepSleep = 30.0;  // in seconds
const cDeepSleepDuration = 86380.0;  // in seconds (24h - 20s)

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
    IDLE,             // 0: Not recording or processing data
    SCAN_RECORD,      // 1: Scanning and recording audio
    SCAN_CAPTURED,    // 2: Processing scan data
    BUTTON_TIMEOUT,   // 3: Timeout limit reached while holding button
    BUTTON_RELEASED,  // 4: Button released, may have audio to send
}


// Globals
gDeviceState <- null; // Hiku device current state
gButtonState <- ButtonState.BUTTON_UP; // Button current state

gButtonTimer <- null; // Handles button-held-too-long
gDeepSleepTimer <- null; // Handles deep sleep timeouts

gScannerOutput <- ""; // String containing barcode data

gAudioBufferOverran <- false; // True if an overrun occurred
gAudioChunkCount <- 0; // Number of audio buffers (chunks) captured
gAudioBufferSize <- 1000; // Size of each audio buffer 
gAudioSampleRate <- 16000; // in Hz

// Each 1k of buffer will hold 1/16 of a second of audio, or 63ms.
// TODO: A-law sampler does not return partial buffers. This means that up to 
// the last buffer size of data is dropped. What size is optimal?
buf1 <- blob(gAudioBufferSize);
buf2 <- blob(gAudioBufferSize);
buf3 <- blob(gAudioBufferSize);
buf4 <- blob(gAudioBufferSize);


//**********************************************************************
//**********************************************************************
// Piezo class
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
    static cycle = 0.5; // 50% duty cycle, maximum power for a piezo
    static longTone = 0.2; // duration in seconds
    static shortTone = 0.15; // duration in seconds

    //**********************************************************
    constructor(hwPin)
    {
        pin = hwPin;

        tonesParamsList = {
            // [[period, duty cycle, duration], ...]
            "success": [[noteE5, cycle, longTone], [noteE6, cycle, shortTone]],
            "failure": [[noteE6, cycle, longTone], [noteE5, cycle, shortTone]],
            "sleep":   [[noteE5, cycle, longTone], [noteB4, cycle, shortTone]],
            "startup": [[noteB4, cycle, longTone], [noteE5, cycle, shortTone]],
        };
    }

    //**********************************************************
    // Play a tone (a set of notes).  Defaults to asynchronous
    // playback, but also supports synchronous via busy waits
    function playSound(tone = "success", async = true) 
    {
        // Handle invalid tone values
        if (!(tone in tonesParamsList))
        {
            server.log("Error: unknown tone");
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
            server.log("ERROR: tried to play null tone");
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


//**********************************************************************
//**********************************************************************
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


//**********************************************************************
// Temporary function to catch dumb mistakes
function print(str)
{
    server.log("ERROR USED PRINT FUNCTION. USE SERVER.LOG INSTEAD.");
}


//**********************************************************************
// Agent callback: upload complete
agent.on(("uploadCompleted"), function(result) {
    //server.log(format("in agent.on uploadCompleted, result=%s", result));
    hwPiezo.playSound(result);
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

    // If we are transitioning to SCAN_RECORD, start the button timer. 
    // If transitioning out of SCAN_RECORD, clear it. The reason 
    // we don't time the actual button press is that, if we have 
    // captured a scan, we don't want to abort.
    if (newState == DeviceState.SCAN_RECORD)
    {
        if (oldState != DeviceState.SCAN_RECORD)
        {
            // Stop timing button press
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


//**********************************************************************
// If we are gathering data and the button has been held down 
// too long, we abort recording and scanning.
function handleButtonTimeout()
{
    updateDeviceState(DeviceState.BUTTON_TIMEOUT);
    stopScanRecord();
    hwPiezo.playSound("failure");
    server.log("Timeout reached. Aborting scan and record.");
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
    hwTrigger.write(1); 

    // Reset for next scan
    gScannerOutput = "";
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

                // If the scan came in late (e.g. after button up), 
                // discard it, to maintain the state machine. 
                if (gDeviceState != DeviceState.SCAN_RECORD)
                {
                    //server.log(format(
                               //"Got capture too late. Dropping scan %d",
                               //gDeviceState));
                    gScannerOutput = "";
                    return;
                }
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
                updateDeviceState(DeviceState.SCAN_RECORD);
                gButtonState = ButtonState.BUTTON_DOWN;
                //server.log("Button state change: DOWN " + gButtonState);

                // Trigger the scanner
                hwTrigger.write(0);

                // Trigger the mic recording
                gAudioBufferOverran = false;
                gAudioChunkCount = 0;
                agent.send("startAudioUpload", "");
                hardware.sampler.start();
            }
            break;
        case 0:
            // Button in released state
            if (gButtonState == ButtonState.BUTTON_DOWN)
            {
                gButtonState = ButtonState.BUTTON_UP;
                //server.log("Button state change: UP " + gButtonState);

                local oldState = gDeviceState;
                updateDeviceState(DeviceState.BUTTON_RELEASED);

                if (oldState == DeviceState.SCAN_RECORD)
                {
                    // Audio captured. Validate and send it

                    // Stop collecting data
                    stopScanRecord();

                    // Tell the server we are done uploading data. We
                    // first wait in case one more chunk comes in. 
                    // TODO: how long to wait??
                    imp.wakeup(gAudioBufferSize/gAudioSampleRate, 
                               notifyAudioUploadComplete);
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



//**********************************************************************
// Tell the server that we are done uploading audio.  This function
// should be called a bit after sampler.stop(), to wait for the last
// chunk to arrive. 
function notifyAudioUploadComplete()
{
    // If there is less than x secs of audio, abandon it
    local secs = gAudioChunkCount*gAudioBufferSize/
                       gAudioSampleRate.tofloat();
    //server.log(format("Captured %0.2f secs of audio in %d chunks", 
    //             secs, gAudioChunkCount));
    if (secs < 0.4) 
    {
        return;
    }

    // If we have bad data, abandon it and play error tone.  Else tell 
    // the server we are done uploading chunks. 
    if (gAudioBufferOverran)
    {
        hwPiezo.playSound("failure");
    }
    else
    {
        agent.send("endAudioUpload", gAudioChunkCount);
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
        gAudioChunkCount++;
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
    imp.configure("hiku", [], []);

    // We will always be in deep sleep unless button pressed, in which
    // case we need to be as responsive as possible. 
    imp.setpowersave(false);

    // Pin assignment
    hwButton <-hardware.pin1;         // Move to IO expander
    hwMicrophone <- hardware.pin2;
    hwPiezo <- Piezo(hardware.pin5);
    hwScannerUart <- hardware.uart57;
    hwTrigger <-hardware.pin8;        // Move to IO expander

    // Pin configuration
    hwButton.configure(DIGITAL_IN_WAKEUP, buttonCallback);
    hwTrigger.configure(DIGITAL_OUT);
    hwTrigger.write(1); // De-assert trigger by default
    hwPiezo.pin.configure(DIGITAL_OUT);
    hwPiezo.pin.write(0); // Turn off piezo by default

    // Microphone sampler config
    hardware.sampler.configure(hwMicrophone, gAudioSampleRate, 
                               [buf1, buf2, buf3, buf4], 
                               samplerCallback, NORMALISE | A_LAW_COMPRESS); 

    // Scanner UART config 
    hardware.configure(UART_57); 
    hwScannerUart.configure(38400, 8, PARITY_NONE, 1, NO_CTSRTS | NO_TX, 
                             scannerCallback);

    // Create our timers
    gButtonTimer = CancellableTimer(cButtonTimeout, handleButtonTimeout);
    gDeepSleepTimer = CancellableTimer(cDelayBeforeDeepSleep,
            function() {
                hwPiezo.playSound("sleep", false /*async*/);
                server.sleepfor(cDeepSleepDuration); 
            });

    // Transition to the idle state
    updateDeviceState(DeviceState.IDLE);

    // Print debug info
    printStartupDebugInfo();

    // Initialization complete notification
    hwPiezo.playSound("startup"); 

    // If we booted with the button held down, we are most likely
    // woken up by the button, so go directly to button handling.  
    // TODO: change when we get the IO expander.  In that case we need
    // to check the interrupt source. 
    buttonCallback();
}


//**********************************************************************
// main
init();

