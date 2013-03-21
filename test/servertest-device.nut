const cFirmwareVersion = "0.5.0";

//**********************************************************************
// Agent callback: upload complete
agent.on(("uploadCompleted"), function(result) {
    server.log(format("DEVICE: upload result=%d", result));
});

//**********************************************************************
// Agent callback: handle external requests
agent.on(("request"), function(request) {
    if (request == "getImpeeId") {
        agent.send("impeeId", hardware.getimpeeid());
    } else {
        server.log(format("Unknown external request %s", request));
    }
});

//**********************************************************************
// Send the beep packet from device to the agent
function sendBeepToAgent(barcode, audio=null) {    
    local beepPacket = {
        serial=hardware.getimpeeid(),
        fw_version=cFirmwareVersion,
        scandata=barcode,
        scansize=barcode.len(),
        linkedrecord="" // not yet implemented
        audiodata=audio,
    };

    agent.send("uploadBeep", beepPacket);
}

//**********************************************************************
// main
server.log("BP Server Test device started");

// Send a barcode
local barcode = "055526130267";
sendBeepToAgent(barcode, "");

// Send binary (fake) audio data 
local audioData = "\x00\x01\x02\x03\x04";
sendBeepToAgent("", audioData);


