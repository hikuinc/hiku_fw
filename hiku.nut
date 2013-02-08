// Copyright 2013 Katmandu Technology, Inc. All rights reserved. Confidential.


// Globals
gScannerOutput <- "";  // String containing barcode data
gButtonState <- "UP";  // Button state ("down", "up") 
                       // TODO: use enumeration/const
gNumBuffersReady <- 0;    // Number of buffers recorded in latest button press

// TODO: A-law sampler does not return partial buffers. This means that up to 
// the last buffer size of data is dropped. What size is optimal?
buf1 <- blob(2000);
buf2 <- blob(2000);
buf3 <- blob(2000);


//****************************************************************************
// Agent callback: doBeep: beep in response to callback
agent.on(("doBeep"), function(msg) {
    server.log(format("in doBeep, msg=%d", msg));
    beep();
    beep();
});



//****************************************************************************
//TODO: review and cleanup
//TODO: instead of reconfiguring, just change duty cycle to zero via
//      pin.write(0). Does this have power impact? 
//TODO: minimize impact of busy wait -- slows responsiveness. Can do 
//      async and maintain sound?
function beep() 
{
    hardware.pin7.configure(PWM_OUT, 0.0015, 0.5);
    imp.sleep(0.2);
    hardware.pin7.configure(PWM_OUT, 0.00075, 0.5);
    imp.sleep(0.2);
    hardware.pin7.configure(DIGITAL_OUT);
    hardware.pin7.write(0);
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
                // do something with the string, and reset state.
                server.log("Code: \"" + gScannerOutput + "\" (" + 
                           gScannerOutput.len() + " chars)");
                beep();
                
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
            if (gButtonState == "UP")
            {
                gButtonState = "DOWN";
                //server.log("Button state change: " + gButtonState);

                // Trigger the scanner
                hardware.pin8.write(0);

                // Trigger the mic recording
                gNumBuffersReady = 0; 
                agent.send("startAudioUpload", "");
                hardware.sampler.start();
            }
            break;
        case numSamples:
            // Button in released state
            if (gButtonState == "DOWN")
            {
                gButtonState = "UP";

                // Release scanner trigger
                hardware.pin8.write(1); 

                // Stop mic recording
                hardware.sampler.stop();
                agent.send("endAudioUpload", "");

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
    gNumBuffersReady++;
    //server.log("SAMPLER CALLBACK: size " + length + " num " + gNumBuffersReady);
    if (length <= 0)
    {
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
    hardware.sampler.configure(hardware.pin5, 16000, [buf1, buf2, buf3], 
                               samplerCallback, NORMALISE | A_LAW_COMPRESS); 
                               //samplerCallback);
                               //samplerCallback, NORMALISE); 
    

    // Scanner UART config 
    hardware.configure(UART_12); 
    hardware.uart12.configure(9600, 8, PARITY_NONE, 1, NO_CTSRTS | NO_TX, 
                             scannerCallback);

    // Initialization complete notification
    server.log(format("Your impee ID=%s", hardware.getimpeeid()));
    beep(); 
}


//****************************************************************************
// main
init();

