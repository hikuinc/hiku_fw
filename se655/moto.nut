// electric imp device code
server.log("Device Started");
 
function scannerData() {
    triggerPin.write(1);
    local b = motorola.read();
    while(b != -1) {
        local state = "Unknown";
        if (b == 0x10) state = "Off";
        if (b == 0x11) state = "On"
        server.log("readFromScanner " + b);
        b = motorola.read();
    }
}

function buttonFunction()
{
  server.log("Trigger Pin pressed");
  triggerPin.write(0);
  imp.wakeup(1, function()
  {
    triggerPin.write(1);
  });
}

function initScanner()
{
   local cmd = "000004E40004FF14"
   server.log("Starting to write to Scanner");
   motorola.write(0x00);
   motorola.write(0x00);
   motorola.write(0x04);
   motorola.write(0xE4);
   motorola.write(0x00);
   motorola.write(0x04);
   motorola.write(0xFF);
   motorola.write(0x14);
}

// Configure the UART 57 
hardware.configure(UART_57);
// Stash the UART object as motorola
motorola <- hardware.uart57;
motorola.configure(9600, 8, PARITY_NONE, 1, NO_CTSRTS, 
                                 scannerData.bindenv(this)); 

hardware.pin2.configure(DIGITAL_OUT);
hardware.pin1.configure(DIGITAL_IN_PULLUP, buttonFunction.bindenv(this));
triggerPin <- hardware.pin2;
triggerPin.write(1);
buttonPin <- hardware.pin1;
/*
function startLooping()
{
    initScanner();
    imp.wakeup(3, startLooping);
}
*/


 
