// Copyright 2013 Katmandu Technology, Inc. All rights reserved. Confidential.


// Consts and enums
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
gDeviceState <- DeviceState.IDLE;  // Hiku device current state
gButtonState <- ButtonState.BUTTON_UP;  // Button current state
gScannerOutput <- "";  // String containing barcode data
gAudioBufferOverran <- false;

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


//****************************************************************************
// Agent callback: upload complete
agent.on(("uploadCompleted"), function(msg) {
    server.log(format("in agent.on uploadCompleted, msg=%s", msg));
    beep(msg);
});


//****************************************************************************
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
            hardware.pin7.configure(PWM_OUT, 0.0015, 0.5);
            imp.sleep(0.2);
            hardware.pin7.configure(PWM_OUT, 0.00075, 0.5);
            imp.sleep(0.2);
            hardware.pin7.configure(DIGITAL_OUT);
            hardware.pin7.write(0);
            break;
        case "failure":
            hardware.pin7.configure(PWM_OUT, 0.00075, 0.5);
            imp.sleep(0.2);
            hardware.pin7.configure(PWM_OUT, 0.0015, 0.5);
            imp.sleep(0.2);
            hardware.pin7.configure(DIGITAL_OUT);
            hardware.pin7.write(0);
            break;
        case "info":
            hardware.pin7.configure(PWM_OUT, 0.00075, 0.5);
            imp.sleep(0.2);
            hardware.pin7.configure(DIGITAL_OUT);
            hardware.pin7.write(0);
            break;
        case "startup":
            hardware.pin7.configure(PWM_OUT, 0.0015, 0.5);
            imp.sleep(0.2);
            hardware.pin7.configure(PWM_OUT, 0.0015, 0.5);
            imp.sleep(0.2);
            hardware.pin7.configure(PWM_OUT, 0.00075, 0.5);
            imp.sleep(0.2);
            hardware.pin7.configure(DIGITAL_OUT);
            hardware.pin7.write(0);
            break;
        default:
            server.log(format("Unknown beep tone requested: \"%s\"", msg));
            break;
    }
}


//****************************************************************************
// UART12 callback, called whenever there is data from scanner
// Reads the bytes, and detects and handles a full barcode string
function scannerCallback()
{
    // Read the first byte
    local data = hardware.uart12.read();
    while (data != -1)  
    {
        //server.log("char " + data + " \"" + data.tochar() + "\"");

        // Handle the data
        switch (data) 
        {
            case '\n':
                // Scan complete. Discard the line ending,
                // upload the beep, and reset state.
                gDeviceState = DeviceState.SCAN_CAPTURED;

                //server.log("Code: \"" + gScannerOutput + "\" (" + 
                           //gScannerOutput.len() + " chars)");
                agent.send("uploadBeep", {barcode=gScannerOutput});
                
                // Release scanner trigger
                hardware.pin8.write(1); 

                // Stop mic recording
                hardware.sampler.stop();

                // Reset for next scan
                gScannerOutput = "";
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
        data = hardware.uart12.read();
    } 

    //server.log("EXITING CALLBACK");
}


//****************************************************************************
// Button handler callback 
// Not a true interrupt handler, this cannot interrupt other Squirrel code. 
// The event is queued and the callback is called next time the Imp is idle. 
function buttonCallback()
{
    // Sample the button multiple times to debounce. Total time 
    // taken is (numSamples-1)*sleepSecs
    const numSamples = 4;
    const sleepSecs = 0.003; 
    local buttonState = hardware.pin9.read()
    for (local i=1;i<numSamples;i++)
    {
        buttonState += hardware.pin9.read()
        imp.sleep(sleepSecs)
    }

    // Handle the button state transition
    switch(buttonState) 
    {
        case 0:
            // Button in held state
            if (gButtonState == ButtonState.BUTTON_UP)
            {
                if (gDeviceState != DeviceState.IDLE)
                {
                    server.log(format("ERROR: expected IDLE, got state %d", 
                               gDeviceState));
                }

                gDeviceState = DeviceState.SCAN_RECORD;
                gButtonState = ButtonState.BUTTON_DOWN;
                //server.log("Button state change: " + gButtonState);

                // Trigger the scanner
                hardware.pin8.write(0);

                // Trigger the mic recording
                gAudioBufferOverran = false;
                agent.send("startAudioUpload", "");
                hardware.sampler.start();
            }
            break;
        case numSamples:
            // Button in released state
            if (gButtonState == ButtonState.BUTTON_DOWN)
            {
                gButtonState = ButtonState.BUTTON_UP;

                switch (gDeviceState) 
                {
                    case DeviceState.BUTTON_TIMEOUT:
                    case DeviceState.SCAN_CAPTURED:
                        // Handling complete, so go straight to idle
                        gDeviceState = DeviceState.IDLE;
                        break;

                    case DeviceState.SCAN_RECORD:
                        // Audio captured. Validate and send it
                        gDeviceState = DeviceState.BUTTON_RELEASED;

                        // Release scanner trigger
                        hardware.pin8.write(1); 

                        // Stop mic recording
                        hardware.sampler.stop();

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
                        gDeviceState = DeviceState.IDLE;
                        break;

                    default:
                        // IDLE, BUTTON_RELEASED, or unknown
                        server.log("ERROR: unexpected state %d", gDeviceState)
                        gDeviceState = DeviceState.IDLE;
                        break;
                }

                //server.log("Button state change: " + gButtonState);
            }
            break;
        default:
            // Button is in transition (not settled)
            //server.log("Bouncing! " + buttonState);
            break;
    }
}


//****************************************************************************
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


//****************************************************************************
function init()
{
    imp.configure("hiku", [], [])
    imp.setpowersave(false);

    // Pin configuration
    hardware.pin9.configure(DIGITAL_IN_PULLUP, buttonCallback); // Button
    hardware.pin8.configure(DIGITAL_OUT); // Scanner trigger
    hardware.pin8.write(1); // De-assert trigger by default
    hardware.pin7.configure(DIGITAL_OUT); // Piezo 
    hardware.pin7.write(0); // Turn off by default

    // Microphone sampler config
    hardware.sampler.configure(hardware.pin5, 16000, [buf1, buf2, buf3, buf4], 
                               samplerCallback, NORMALISE | A_LAW_COMPRESS); 
                               //samplerCallback);
                               //samplerCallback, NORMALISE); 
    

    // Scanner UART config 
    hardware.configure(UART_12); 
    hardware.uart12.configure(9600, 8, PARITY_NONE, 1, NO_CTSRTS | NO_TX, 
                             scannerCallback);

    // Initialization complete notification
    server.log(format("Your impee ID=%s", hardware.getimpeeid()));
    beep("startup"); 
}


//****************************************************************************
// main
init();
