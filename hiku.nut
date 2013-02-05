// Copyright 2013 Katmandu Technology, Inc. All rights reserved. Confidential.


// Globals
gAudioOut <- OutputPort("Audio", "string");  // Test audio output
gScannerOutput <- "";  // String containing barcode data
gButtonState <- "UP";  // Button state ("down", "up") 
                       // TODO: use enumeration/const
gNumBuffersReady <- 0;    // Number of buffers recorded in latest button press

// Audio buffers   (TODO: review)
buf1 <- blob(2000);
buf2 <- blob(2000);
buf3 <- blob(2000);


//****************************************************************************
//TODO: review and cleanup
//TODO: instead of reconfiguring, just change duty cycle to zero via
//      pin.write(0). Does this have power impact? 
//TODO: minimize impact of busy wait -- slows responsiveness. Can do 
//      async and maintain sound?
//TODO: support different sounds for boot, scan success, scan fail, 
//      product found, product not found (via table of ([1/Hz],[duration]))
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
                server.log("Button state change: " + gButtonState);
                server.log(format("Free memory: %d", imp.getmemoryfree()));

                // Trigger the scanner
                hardware.pin8.write(0);

                // Trigger the mic recording
                gNumBuffersReady = 0; 
                hardware.sampler.start();
            }
            break;
        case numSamples:
            // Button in released state
            if (gButtonState == "DOWN")
            {
                gButtonState = "UP";
                server.log(format("Free memory up: %d", imp.getmemoryfree()));

                // Release scanner trigger
                hardware.pin8.write(1); 

                // Stop mic recording
                hardware.sampler.stop();

                server.log("Button state change: " + gButtonState);
                server.log(format("Free memory stop: %d", imp.getmemoryfree()));
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
    server.log("SAMPLER CALLBACK: size " + length + " num " + gNumBuffersReady);
    if (length <= 0)
    {
        server.log("Audio sampler buffer overrun!!!!!!");
    }
    else 
    {
        // Output the buffer 
        //gAudioOut.set(buffer);
        while(!buffer.eos())
        {
            server.log(buffer.readn('c'));
        }

        gAudioOut.set(format("Got %d samples", gNumBuffersReady));
    }
}


//****************************************************************************
function init()
{
    imp.configure("hiku", [], [gAudioOut])
    imp.setpowersave(false);

    // Pin configuration
    hardware.pin9.configure(DIGITAL_IN_PULLUP, buttonCallback); // Button
    hardware.pin8.configure(DIGITAL_OUT); // Scanner trigger

    // Microphone sampler config
    hardware.sampler.configure(hardware.pin5, 16000, [buf1, buf2, buf3], 
                               samplerCallback, NORMALISE | A_LAW_COMPRESS); 
    

    // Scanner UART config 
    hardware.configure(UART_12); 
    hardware.uart12.configure(9600, 8, PARITY_NONE, 1, NO_CTSRTS | NO_TX, 
                             scannerCallback);

    // Initialization complete notification
    beep(); 
}


//****************************************************************************
// main
init();

