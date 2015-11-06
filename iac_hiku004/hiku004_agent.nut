// Copyright 2013 Katmandu Technology, Inc. All rights reserved. Confidential.

enum AudioStates {
  AudioRecording,  // Hiku's button has been pressed and audio is being recorded.
  AudioFinished,   // The button has been released and the audio has to be interpreted by
                   // the server. This is the case if only audio was recorded or if the audio
           // is to add an entry to an unknown barcode (superscan).
  AudioFinishedBarcode, // The button has been released and the server can ignore the audio
  AudioError       // The most recent audio recording encountered an error. This state is also
                   // used on agent initialization.
}

if (!("nv" in getroottable()))
{
    nv <- { 
            gImpeeId = "", 
            gChargerState = "removed" , 
            gLinkedRecordTimeout = null,
            gBootTime = 0.0,
            gSleepDuration = 0.0,
            gWakeUpReason = 0x0000,
            gBatteryLevel = 0.0,
            gFwVersion = 0.0,
            at_factory = false,
            macAddress = null,
            countryCode = null,
            deviceReady = null
          };
}

// use a round robin table to do logging
const MAX_TABLE_COUNT = 5; // going to have 5 buffers
const MAX_LOG_COUNT = 30;
gLogTableIndex <- 0;
gLogTable <- [{count=0,data=""},
          {count=0,data=""},
          {count=0,data=""},
          {count=0,data=""},
          {count=0,data=""}];

server.log(format("Agent started, external URL=%s at time=%ds", http.agenturl(), time()));

gAgentVersion <- "2.1.04";

gAudioState <- AudioStates.AudioError;
gAudioAbort <- false;

// Byte pointer indicating the number of bytes of audio data
// the server has picked up from the Imp agent with HTTP GETs
gAudioReadPointer <- 0;

// Audio token issued by the server as an identifier for the 
// transaction
gAudioToken <- "";

// Global variable for measuring transaction time of list POSTs 
gTransactionTime <- 0;

// Byte string containing the audio data for the current session
gAudioString <- "";

gChunkCount <- 0; // Used to verify that we got the # we expected
gLinkedRecord <- ""; // Used to link unknown UPCs to audio records. 
                     // We will set this when we get a request to do
                     // so from the server, then clear it after we 
                     // send the next audio beep, after next barcode, 
                     // or after a timeout
                     
gEndUploadData <- null;

gEan <- ""; // stored EAN for a superscan
                     
local boot_reasons = [
                        "accelerometer", 
                        "charger_status", 
                        "button", 
                        "touch", 
                        "not-used", 
                        "not-used",
                        "not-used",
                        "charger-det", 
                      ];

gAuthData <-{
                app_id="e3a8ccb635d08ce76b407ec644",
                secret="c923b1e09386",
            };
            
gMixPanelData <- {
            url="https://api.mixpanel.com/track/",
            token="d4dca732e8c83981405c87c65ae3e834",
         };

const MIX_PANEL_EVENT_DEVICE_BUTTON = "DeviceButton";
const MIX_PANEL_EVENT_BARCODE = "DeviceScan";
const MIX_PANEL_EVENT_SPEAK = "DeviceSpeak";
const MIX_PANEL_EVENT_CHARGER = "DeviceCharger";
const MIX_PANEL_EVENT_BATTERY = "DeviceBatteryLevel";
const MIX_PANEL_EVENT_CONNECTIVITY = "DeviceConnectivity";
const MIX_PANEL_EVENT_CONFIG = "DeviceConfig";
const MIX_PANEL_EVENT_STATUS = "DeviceStatus";

// Heroku server base URL   
//gBaseUrl <- "https://app-staging.hiku.us/api/v1";
gBaseUrl <- "https://app.hiku.us/api/v1";
gFactoryUrl <- "https://hiku-mfgdb.herokuapp.com/api/v1";

gServerUrl <- gBaseUrl + "/list";   

gBatteryUrl <- gBaseUrl + "/device";

gLogUrl <- gBaseUrl + "/log";

gSetupUrl <- gBaseUrl + "/apps";

gAudioUrl <- gBaseUrl + "/audio";

const BATT_0_PERCENT = 43726.16;

// prefixes indicating special barcodes with minimum
// and maximum barcode lengths in characters
const PREFIX_IMP_MAC = "0c2a69";
const PREFIX_IMP_LABELING = ".HCMGETMAC";
const PREFIX_GENERAL = ".HFB";
const PREFIX_OBA_CHECK = ".HCOBACHECK";

const TEST_CMD_PACKAGE = "package";
const TEST_CMD_PRINT_LABEL = "label";
const TEST_CMD_OBA_CHECK = "obacheck";


const IMAGE_COLUMNS = 1016;
scanned_image <- "";
lines_scanned <- 0;

gSpecialBarcodePrefixes <- [{
    // MAC addresses of Electric Imp modules printed as barcodes at the factory
    prefix = PREFIX_IMP_MAC,
    min_len = 12,
    max_len = 12,
    url = gFactoryUrl + "/factory"},{
    // this is to check the mac address at the oba station
    prefix = PREFIX_OBA_CHECK,
    min_len = 11,
    max_len = 11,
    url = gFactoryUrl + "/factory"}, {
    // barcode for generating a 2D datamatrix barcode for the scanner at the
        // label printer in production  
    prefix = PREFIX_IMP_LABELING,
    min_len = 10,
    max_len = 10,
    url = gFactoryUrl + "/factory"},{
    // general special barcodes
    prefix = PREFIX_GENERAL,
    min_len = 5,
    max_len = 64,
    url = null
    }];

//======================================================================
function sendMixPanelEvent(event, parameters)
{
/*
  // Disabled for now
  local defaultParam = (parameters)?parameters:{};
  defaultParam["token"] <- gMixPanelData.token;
  defaultParam["distinct_id"] <- nv.gImpeeId;
  defaultParam["time"] <- time();
  
  local data = {
        "event":event,
        "properties":defaultParam,
         };
     
  local request = {data=http.base64encode(http.jsonencode(data))};
  
    // Create and send the request
  local req = http.post(
            gMixPanelData.url,
            {"Content-Type": "application/x-www-form-urlencoded", 
            "Accept": "application/json"}, 
            http.urlencode(request));
            
    // If the server is down, this will block all other events
    // until it times out.  Events seem to be queued on the server 
    // with no ill effects.  They do not block the device.  Could consider 
    // moving to async. The timeout period (tested) is 60 seconds.  
    req.sendasync(onMixPanelEventComplete);
  */
}

function onMixPanelEventComplete(m)
{
    if (m.statuscode != 200)
    {
        server.log(format("AGENT: MixPanelEvent: Error: got status code %d, expected 200", 
                    m.statuscode));
    }
    else
    {
      local body = http.jsondecode(m.body);
      server.log(body);
      server.log("MixPanelEventSuccess");
    }
}

//======================================================================
// Beep handling 

// Send Device Status to Hiku Server
function sendDeviceEvents(data)
{
    local disableSendToServer = false;
    //disableSendToServer = true;
    if (disableSendToServer)
    {
        server.log("AGENT: (sending to hiku server not enabled)");
        return;
    }
   
    // URL-encode the whole thing
    //data = http.urlencode(data);
    
    local timeStr = getUTCTime();
    local mySig = http.hash.sha256(gAuthData.app_id+gAuthData.secret+timeStr);
    mySig = BlobToHexString(mySig);       
    
    data = {    
            "sig": mySig,
            "time": timeStr, 
            "app_id": gAuthData.app_id,
            serialNumber = nv.gImpeeId,
             logData = http.jsonencode(data) };
                 
    data = http.urlencode( data );
    server.log("AGENT: "+data);


    // Create and send the request
    server.log("AGENT: Sending Event to server...");
    local req = http.post(
            //"http://199.115.118.221/cgi-bin/addDeviceEvent.py",
            //"http://srv2.hiku.us/cgi-bin/addDeviceEvent.py",
            gLogUrl,
            {"Content-Type": "application/x-www-form-urlencoded", 
            "Accept": "application/json"}, 
            data);

    req.sendasync(onComplete);
}

function sendBatteryLevelToHikuServer(data)
{
    local disableSendToServer = false;
    local urlToPut = gBatteryUrl + "/" + nv.gImpeeId;
    local newData;
    
    //sendMixPanelEvent(MIX_PANEL_EVENT_BATTERY,{"batteryLevel":data.batteryLevel});
    
    server.log(format("AGENT: Battery Level URL: %s", urlToPut));
    //disableSendToServer = true;
    if (disableSendToServer)
    {
        server.log("AGENT: (sending to hiku server not enabled)");
        return;
    }
    
    local timeStr = getUTCTime();
    local mySig = http.hash.sha256(gAuthData.app_id+gAuthData.secret+timeStr);
    mySig = BlobToHexString(mySig);    
        
    // URL-encode the whole thing
    //data = http.jsonencode(data);
    
    newData = {
                "batteryLevel":data.batteryLevel,
                "token": nv.gImpeeId,
                "sig": mySig,
                "time": timeStr,              
                "app_id": gAuthData.app_id
              };
    
    data = http.urlencode( newData );
    server.log("AGENT: "+data);

    // Create and send the request
    server.log("AGENT: Sending Event to server...");
    local req = http.put(
            //"http://199.115.118.221/cgi-bin/addDeviceEvent.py",
           // "http://srv2.hiku.us/cgi-bin/addDeviceEvent.py",
           urlToPut,
            {"Content-Type": "application/x-www-form-urlencoded", 
            "Accept": "application/json"}, 
            data);

    req.sendasync(onCompleteEvent);
}

function onCompleteEvent(m)
{
    if (m.statuscode != 200)
    {
        server.log(format("AGENT: Battery Event: Error: got status code %d, expected 200", 
                    m.statuscode));
    }
    else
    {

        // Parse the response (in JSON format)
        local body = http.jsondecode(m.body);
        local body = body.response;
        try 
        {
            // Handle the various non-OK responses.  Nothing to do for "ok". 
            if (body.status != "ok")
            {
        server.log(format("AGENT: Battery Event - Error: %s", body.errMsg));
            }
        }
        catch(e)
        {
            server.log(format("AGENT: Battery Event - Caught exception: %s", e));
        }
    }
}

function mixPanelEvent(event,extraParam)
{
  local data = {};
  data[event] <- extraParam;
  return data;
}

//**********************************************************************
// Send audio and/or barcode to hiku's server
function sendBeepToHikuServer(data)
{
    //server.log(format("AGENT: Audio get at %d", gAudioReadPointer));

    local disableSendToServer = false;
    local newData = null;
    
    local is_superscan = false;
    //disableSendToServer = true;
    if (disableSendToServer)
    {
        agentLog("(sending to hiku server not enabled)");
        gAudioState = AudioStates.AudioError;
        return;
    }
    
    if(data.scandata != "") {
    if (handleSpecialBarcodes(data)) {
        if (gAudioState != AudioStates.AudioError) {
        gAudioState = AudioStates.AudioFinishedBarcode;
        }
        return;
    }
    }
    
    local timeStr = getUTCTime();
    local mySig = http.hash.sha256(gAuthData.app_id+gAuthData.secret+timeStr);
    mySig = BlobToHexString(mySig);
    
    server.log(format("Current Impee Id=%s Valid ImpeeId=%s, gAudioToken: %s, appId: %s",nv.gImpeeId, data.serial, gAudioToken, gAuthData.app_id));
    nv.gImpeeId = data.serial;
        
    // Special handling for audio beeps
    if (data.scandata == "")
    {
        // If not expired, attach the current linkedrecord (usually 
        // blank). Then reset the global. 
        agentLog("checking if linked record");
        if (nv.gLinkedRecordTimeout && time() < nv.gLinkedRecordTimeout)
        {
            agentLog("record linked");
            data.scandata = gLinkedRecord;
              is_superscan = true;
        }
        gLinkedRecord = ""; 
        nv.gLinkedRecordTimeout = null;
    }
    
    if ( data.scandata == "" )
    {
            newData = {
                        "audioToken": gAudioToken,
                          "audioType": "alaw",
                          "token": nv.gImpeeId,
                    "sig": mySig,
                    "app_id": gAuthData.app_id,
                    "time": timeStr,
                  };  
            if (gAudioState != AudioStates.AudioError) {
          gAudioState = AudioStates.AudioFinished;
          sendMixPanelEvent(MIX_PANEL_EVENT_SPEAK,{"status":"AudioFinished"});
          //sendDeviceEvents(mixPanelEvent(MIX_PANEL_EVENT_SPEAK,{"status":"AudioFinished"}));
        }
    }
    else 
    {
      if (is_superscan) 
      {
          newData = {
            "ean":format("%s",data.scandata),
            "audioToken": gAudioToken,
            "audioType": "alaw",                
            "token": nv.gImpeeId,
            "sig": mySig,
            "app_id": gAuthData.app_id,
            "time": timeStr,
          };   
    if (gAudioState != AudioStates.AudioError) {
      gAudioState = AudioStates.AudioFinished;
      sendMixPanelEvent(MIX_PANEL_EVENT_SPEAK,{"status":"CrowdSourced","barcode":data.scandata});
      //sendDeviceEvents(mixPanelEvent(MIX_PANEL_EVENT_SPEAK,{"status":"CrowdSourced","barcode":data.scandata}));
    }
      } else {
      newData = {
        "ean":data.scandata,
        "audioType": "alaw",                
        "token": nv.gImpeeId,
        "sig": mySig,
        "app_id": gAuthData.app_id,
        "time": timeStr,
      };   
           if (gAudioState != AudioStates.AudioError) {
              gAudioState = AudioStates.AudioFinishedBarcode;
           }
        }
        sendMixPanelEvent(MIX_PANEL_EVENT_BARCODE,{"status":"Scanned", "barcode":data.scandata});
        //sendDeviceEvents(mixPanelEvent(MIX_PANEL_EVENT_BARCODE,{"status":"Scanned", "barcode":data.scandata}));
      // save EAN in case of a superscan
      gEan = newData.ean;
    }
    // URL-encode the whole thing
    newData = http.urlencode(newData);
    // Create and send the request
    agentLog("Sending beep to server...");
    local req = http.post(
            //"http://bobert.net:4444", 
            //"http://www.hiku.us/sand/cgi-bin/readRawDeviceData.py", 
            //"http://199.115.118.221/scanner_1/imp_beep",
            gServerUrl,
            {"Content-Type": "application/x-www-form-urlencoded", 
            "Accept": "application/json"}, 
            newData);
            
    // If the server is down, this will block all other events
    // until it times out.  Events seem to be queued on the server 
    // with no ill effects.  They do not block the device.  Could consider 
    // moving to async. The timeout period (tested) is 60 seconds.  
    gTransactionTime = time();
    req.sendasync(onBeepReturn);
}

function onBeepReturn(res) {

    gTransactionTime = time() - gTransactionTime;
    agentLog(format("Server transaction time: %ds", gTransactionTime));

    // Handle the response
    local returnString = "success-server";

    if (res.statuscode != 200)
    {
        returnString = "failure"
        agentLog(format("Beep Error: got status code %d, expected 200", 
                    res.statuscode));
    }
    else
    {
        // Parse the response (in JSON format)
        local body = http.jsondecode(res.body);
        local body = body.response;
        try 
        {
            // Handle the various non-OK responses.  Nothing to do for "ok". 
            if (body.status != "ok")
            {
                // Possible causes: speech2text failure, unknown.
                if (body.errMsg == "EAN_NOT_FOUND")
                {
                    gLinkedRecord = gEan;
                    nv.gLinkedRecordTimeout = time()+10; // in seconds
                    returnString = "unknown-upc";
                    agentLog("Response: unknown UPC code");
                }
                else
                {
                    returnString = "failure";
                }
                agentLog(format("Beep Error: %s",http.jsonencode(body)));
            }
        }
        catch(e)
        {
            agentLog(format("Beep Error: Caught exception: %s", e));
            returnString = "failure";
        }
    }

    // Return status to device
    // TODO: device.send will be dropped if response took so long that 
    // the device went back to sleep.  Handle that?
    local event = (gAudioState == AudioStates.AudioFinished)?MIX_PANEL_EVENT_SPEAK:MIX_PANEL_EVENT_BARCODE;
    sendMixPanelEvent(event,{"barcode":gEan,"status":returnString});
    //sendDeviceEvents(mixPanelEvent(event,{"barcode":gEan,"status":returnString}));
    device.send("uploadCompleted", returnString);
}

// Send the barcode to hiku's server
function handleSpecialBarcodes(data)
{
    local barcode = data.scandata;
    local req = null;
    local is_special = false;
    local dataToSend = null;
    // test the barcode against all known special barcode prefixes
    for (local i=0; i<gSpecialBarcodePrefixes.len() && !is_special; i++) {
    local prefix = gSpecialBarcodePrefixes[i]["prefix"];
    local min_len = gSpecialBarcodePrefixes[i]["min_len"];
    local max_len = gSpecialBarcodePrefixes[i]["max_len"];
    local barcode_len = barcode.len();
    if ((barcode_len >= min_len) && (barcode_len <= max_len)) {
        local temp = barcode.slice(0,prefix.len());
        server.log("Original Barcode: "+barcode+" Sliced Barcode: "+temp);
        if (temp == prefix)
        switch (prefix) {
        case PREFIX_IMP_MAC :
        server.log(format("Scanned MAC %s", barcode));
        if (!nv.at_factory) {
            return false;
        }
        dataToSend = {
            "macAddress": nv.macAddress,
            "serialNumber": nv.gImpeeId,
            "scannedMacAddress": barcode,
            "command": TEST_CMD_PACKAGE
                };
        is_special = true;
        local json_data = http.jsonencode (dataToSend);
        server.log(json_data);
        req = http.post(
            gSpecialBarcodePrefixes[i]["url"],
            {"Content-Type": "application/json", 
            "Accept": "application/json"}, 
            json_data);
        break;
        
        case PREFIX_OBA_CHECK:
          server.log(format("Scanned label code %s", barcode));
          if (!nv.at_factory) {
              return false;
          }
          is_special = true;
          dataToSend = {
              "macAddress": nv.macAddress,
              "serialNumber": nv.gImpeeId,
              "agentUrl": http.agenturl(),
              "command": TEST_CMD_OBA_CHECK
              };
          sendMixPanelEvent(MIX_PANEL_EVENT_CONFIG,dataToSend);
          //sendDeviceEvents(mixPanelEvent(MIX_PANEL_EVENT_CONFIG,dataToSend));
          local json_data = http.jsonencode (dataToSend);
          server.log(json_data);
          req = http.post(
              gSpecialBarcodePrefixes[i]["url"],
              {"Content-Type": "application/json", 
              "Accept": "application/json"}, 
              json_data);
          break;        
        
        case PREFIX_IMP_LABELING :
        server.log(format("Scanned label code %s", barcode));
        if (!nv.at_factory) {
            return false;
        }
        is_special = true;
        dataToSend = {
            "macAddress": nv.macAddress,
            "serialNumber": nv.gImpeeId,
            "agentUrl": http.agenturl(),
            "command": TEST_CMD_PRINT_LABEL
                };
        sendMixPanelEvent(MIX_PANEL_EVENT_CONFIG,dataToSend);
        //sendDeviceEvents(mixPanelEvent(MIX_PANEL_EVENT_CONFIG,dataToSend));
        local json_data = http.jsonencode (dataToSend);
        server.log(json_data);
        // HACK HACK HACK
        local printer_req = http.get("https://agent.electricimp.com/WQ8othFzM2Zm/printMAC?mac="+nv.macAddress, {});
        printer_req.sendasync(onMacPrintReturn);
        // Goes to test controller 20000c2a69093434
        //local iac_printer_req = http.get("https://agent.electricimp.com/TvVLVemS7PR9/printMAC?mac="+nv.macAddress+"&countryCode="+nv.countryCode, {});
        // Goes to test controller 20000c2a69090783
        local iac_printer_req = http.get("https://agent.electricimp.com/3qMq5k6INLiw/printMAC?mac="+nv.macAddress+"&countryCode="+nv.countryCode, {});
        iac_printer_req.sendasync(onMacPrintReturn);
        req = http.post(
            gSpecialBarcodePrefixes[i]["url"],
            {"Content-Type": "application/json", 
            "Accept": "application/json"}, 
            json_data);
        break;
        case PREFIX_GENERAL :
        server.log(format("Scanned special barcode %s", barcode));
        is_special = true;
        local timeStr = getUTCTime();
        local mySig = http.hash.sha256(gAuthData.app_id+gAuthData.secret+timeStr);
        mySig = BlobToHexString(mySig);
        
        server.log(format("Current Impee Id=%s Valid ImpeeId=%s",nv.gImpeeId, data.serial));
        nv.gImpeeId = data.serial;
        
        local newData = {
                "frob":data.scandata,               
                "token": nv.gImpeeId,
                    "sig": mySig,
                    "app_id": gAuthData.app_id,
                    "time": timeStr,
                    "serialNumber": nv.gImpeeId,
            };
        
        dataToSend = {"frob":data.scandata, "command":"ExternalAppAuthentication"};
        local url = gSetupUrl+"/"+data.scandata;
        server.log("Put URL: "+url);
        // URL-encode the whole thing
        dataToSend = http.urlencode(newData);
        server.log(dataToSend);
        req = http.put(
            url,
            {"Content-Type": "application/x-www-form-urlencoded", 
            "Accept": "application/json"}, 
            dataToSend);
        break;
        }
    }
    }
    if (!is_special)
    return false;
    
    sendMixPanelEvent(MIX_PANEL_EVENT_CONFIG,dataToSend);
    //sendDeviceEvents(mixPanelEvent(MIX_PANEL_EVENT_CONFIG,dataToSend));   
    // Create and send the request
    agentLog("Sending special barcode to server...");
            
    // If the server is down, this will block all other events
    // until it times out.  Events seem to be queued on the server 
    // with no ill effects.  They do not block the device.  Could consider 
    // moving to async. The timeout period (tested) is 60 seconds.  
    gTransactionTime = time();
    req.sendasync(onSpecialBarcodeReturn);
    return true;
}

function onMacPrintReturn(res) {
}

function onSpecialBarcodeReturn(res) {
    gTransactionTime = time() - gTransactionTime;
    agentLog(format("Server transaction time: %ds", gTransactionTime));

    // Handle the response
    local returnString = "success-server";

    if (res.statuscode != 200)
    {
        returnString = "failure"
        agentLog(format("Error: got status code %d, expected 200", 
                    res.statuscode));
    }
    else
    {
        // Parse the response (in JSON format)
        local body = http.jsondecode(res.body);
        local body = body.response;
        try 
        {
            // Handle the various non-OK responses.  Nothing to do for "ok". 
            if (body.status != "ok")
            {
                returnString = "failure";
                agentLog(format("AGENT: Beep Error: %s",http.jsonencode(body)));
            }
        }
        catch(e)
        {
            agentLog(format("Caught exception: %s", e));
            returnString = "failure";
        }
    }

    // Return status to device
    // TODO: device.send will be dropped if response took so long that 
    // the device went back to sleep.  Handle that? 
    device.send("uploadCompleted", returnString);
}

function sendLogToServer(data)
{
    local disableSendToServer = false;
    //disableSendToServer = true;
    if (disableSendToServer)
    {
        server.log("AGENT: (sending to hiku server not enabled)");
        return;
    }
    
    
    gLogTable[gLogTableIndex].data +=data+"\n";
    gLogTable[gLogTableIndex].count++;
    
    if( gLogTable[gLogTableIndex].count == MAX_LOG_COUNT)
    {
       local prevIndex = gLogTableIndex;
       
       gLogTableIndex = (gLogTableIndex + 1) % MAX_TABLE_COUNT;
       gLogTable[prevIndex].count = 0;
       data = {log=gLogTable[prevIndex].data};
       gLogTable[prevIndex].data = "";
    }
    else
    {
      server.log(format("still budnling up: Table Index: %d, Log Count: %d",gLogTableIndex,gLogTable[gLogTableIndex].count));
      return;
    }    
        
    // URL-encode the whole thing
   // data = http.urlencode(data);
    local timeStr = getUTCTime();
    local mySig = http.hash.sha256(gAuthData.app_id+gAuthData.secret+timeStr);
    mySig = BlobToHexString(mySig);
    data = {    
            "sig": mySig,
            "time":timeStr,
            "app_id": gAuthData.app_id,
            serialNumber = nv.gImpeeId,
            logData = http.jsonencode(data)
           };
    data = http.urlencode(data);
    server.log("sendToLogServer: "+data);

    // Create and send the request
    local req = http.post(
            //"http://199.115.118.221/cgi-bin/addDeviceLog.py",
            //"http://srv2.hiku.us/cgi-bin/addDeviceLog.py",
            gLogUrl,
            {"Content-Type": "application/x-www-form-urlencoded", 
            "Accept": "application/json"}, 
            data);
            
    // If the server is down, this will block all other events
    // until it times out.  Events seem to be queued on the server 
    // with no ill effects.  They do not block the device.  Could consider 
    // moving to async. The timeout period (tested) is 60 seconds.  
    req.sendasync(onComplete);
}

function onComplete(m)
{
    if (m.statuscode != 200)
    {
        server.log(format("AGENT: Log Status: Error: got status code %d, expected 200", 
                    m.statuscode));
    }
    else
    {

        // Parse the response (in JSON format)
        local body = http.jsondecode(m.body);
        local body = body.response;
        try 
        {
            // Handle the various non-OK responses.  Nothing to do for "ok". 
            // Handle the various non-OK responses.  Nothing to do for "ok". 
            //dumpTable(body);
            if (body.status != "ok")
            {
                server.log(format("AGENT: Log Status - Error: %s", body.errMsg));
            }
            else
            {
                server.log(format("AGENT: Log Status Success: %s", http.jsonencode(body.data)));
            }
        }
        catch(e)
        {
            server.log(format("AGENT: Log Status - Caught exception: %s", e));
        }
    }
}

//**********************************************************************
// Receive and send out the beep packet
device.on("uploadBeep", function(data) {
    gLinkedRecord = "";  // Clear on next (i.e. this) barcode scan
    nv.gLinkedRecordTimeout = null;
    sendBeepToHikuServer(data);  
});

//**********************************************************************
// Receive and send out the beep packet
device.on("batteryLevel", function(data) {

    agentLog(format("Battery Level Raw Reading: %d", 
                   data));  
    if( data >= 59760 ) // >= 4.10V battery voltage
    {
        data = 100;
    }
    else if ( data < 59760 && data >= 59031 ) // >= 4.05 V battery voltage
    {
        data = 95;
    }   
    else if ( data < 59031 && data >= 58302 ) // >= 4.00V battery voltage
    {
        data = 90;
    }   
    else if ( data < 58302 && data >= 57719 ) // >= 3.96V battery voltage
    {
        data = 85;
    }
    else if ( data < 57719 && data >= 57136 ) // >= 3.92V battery voltage
    {
        data = 80;
    }
    else if ( data < 57136 && data >= 56699 ) // >= 3.89V battery voltage
    {
        data = 75;
    }
    else if ( data < 56699 && data >= 56407 ) // >= 3.87V battery voltage
    {
        data = 70;
    }
    else if ( data < 56407 && data >= 56043 ) // >= 3.845V battery voltage
    {
        data = 65;
    }
    else if ( data < 56043 && data >= 55678 ) // >= 3.82V battery voltage
    {
        data = 60;
    }
    else if ( data < 55678 && data >= 55460 ) // >= 3.805V battery voltage
    {
        data = 55;
    }   
    else if ( data < 55460 && data >= 55241 ) // >= 3.79V battery voltage
    {
        data = 50;
    } 
    else if ( data < 55241 && data >= 54950 ) // >= 3.77V battery voltage
    {
        data = 45;
    }
    else if( data < 54950 && data >= 54658 ) // >= 3.75V battery voltage
    {
        data = 40;
    }
    else if( data < 54658 && data >= 54294 ) // >= 3.725V battery voltage
    {
        data = 35;
    }
    else if( data < 54294 && data >= 53929 ) // >= 3.70V battery voltage
    {
        data = 30;
    }
    else if( data < 53929 && data >= 53565 ) // >= 3.675V battery voltage
    {
        data = 25;
    }   
    else if( data < 53565 && data >= 53200 ) // >= 3.65V battery voltage
    {
        data = 20;
    }
    else if( data < 53200 && data >= 52763 ) // >= 3.62V battery voltage
    {
        data = 15;
    }
    else if( data < 52763 && data >= 51014) // >= 3.5V battery voltage
    {
        data = 10;
    }   
    else if( data < 51014 && data >= 50285 ) // >= 3.45V battery voltage
    {
        data = 5;
    }           
    else if( data < 50285 ) // <3.40V battery voltage
    {
        // This means we are below 5% and its 43726.16722 for 0%
        // Perhaps we should give finer granular percentage here until it hits 1% to 0%
        data = 1;
    }

    nv.gBatteryLevel = data;

    sendBatteryLevelToHikuServer({batteryLevel=data});
    sendMixPanelEvent(MIX_PANEL_EVENT_BATTERY,{"status":nv.gBatteryLevel});
    
});


//**********************************************************************
// Prepare to receive audio from the device
device.on("startAudioUpload", function(data) {
    //agentLog("in startAudioUpload");
    sendMixPanelEvent(MIX_PANEL_EVENT_SPEAK,{"status":"StartAudio"});
    //sendDeviceEvents(mixPanelEvent(MIX_PANEL_EVENT_SPEAK,{"status":"StartAudio"}));
    // Reset our audio buffer
    // HACK
    // HACK
    // HACK
    // Have to correctly handle the case where the user presses the button
    // again to submit a second audio recording while the data of the first
    // recording hasn't yet been sent to the server. This can be the case
    // if audio data is still on the device or if the final HTTP GET from the server
    // to pick up the audio data is still in flight.
    // 
    // Currently, the audio data for the first recording gets destroyed when the
    // user presses the button again.
    gAudioToken = "";
    gChunkCount = 0;
    gAudioReadPointer = 0;
    gAudioString = "";
    gAudioAbort = false;
    
    // POST to the server to indicate that a new audio recording is
    // starting; receive an audio token in return
    local newData;
    local timeStr = getUTCTime();
    local mySig = http.hash.sha256(gAuthData.app_id+gAuthData.secret+timeStr);
    mySig = BlobToHexString(mySig);
    
    newData = {
          // FIXME don't send agent URL in the clear over HTTP
          "agentUrl": http.agenturl(),
          "agentAudioRate": 8000,
          "token": nv.gImpeeId,
          "sig": mySig,
          "app_id": gAuthData.app_id,
          "time": timeStr,
    };      
    
    data = http.urlencode( newData );
    
    server.log("AGENT: Posting audio start to server...");
    local req = http.post(
            //"http://199.115.118.221/cgi-bin/addDeviceEvent.py",
            //"http://srv2.hiku.us/cgi-bin/addDeviceEvent.py",
            gAudioUrl,
            {"Content-Type": "application/x-www-form-urlencoded", 
            "Accept": "application/json"}, 
            data);

    req.sendasync(onReceiveAudioToken);
});

function onReceiveAudioToken(m)
{
    if (m.statuscode != 200)
    {
        agentLog(format("Token POST: Got status code %d, expected 200.", m.statuscode));
        gAudioState = AudioStates.AudioError;
    }
    else
    {
      // Parse the response (in JSON format)
        local body = http.jsondecode(m.body);
        local body = body.response;
        try 
        {
          if (body.status != "ok") {
            gAudioState = AudioStates.AudioError;
            agentLog("Token POST: Received non-ok status");
          } else {  
            gAudioToken = body.data.audioToken;
            if (gEndUploadData)
            {
                imp.wakeup(0.001,waitUntilTokenReceived);
            }
            agentLog(format("Token POST: AudioToken received: %s", gAudioToken));
            gAudioState = gAudioAbort ? AudioStates.AudioError : AudioStates.AudioRecording;
          }
        }
        catch(e)
        {
            agentLog(format("Token POST: Caught exception: %s", e));
            gAudioState = AudioStates.AudioError;
        }
    }
}

device.on("button",function(str){
  server.log(str);
  sendMixPanelEvent(MIX_PANEL_EVENT_DEVICE_BUTTON,{"status":str});
  sendDeviceEvents(mixPanelEvent(MIX_PANEL_EVENT_DEVICE_BUTTON,str));
});

device.on("usbState",function(str){
  server.log(str);
  sendMixPanelEvent(MIX_PANEL_EVENT_CHARGER,{"USBState":str});
  //sendDeviceEvents(mixPanelEvent(MIX_PANEL_EVENT_CHARGER,{"USBState":str}));
});

device.on("deviceLog", function(str){
    // this needs to be changed post to an http url
    server.log(format("DEVICE: %s",str));
    sendLogToServer(format("%s DEVICE: %s",getUTCTime(), str));
});

//**********************************************************************
// Send complete audio sample to the server
device.on("abortAudioUpload", function(data) {
    agentLog("Error: aborting audio upload");
    gAudioAbort = true;
    gAudioState = AudioStates.AudioError;
});
  
//**********************************************************************
// Send complete audio sample to the server
device.on("endAudioUpload", function(data) {
    //agentLog("in endAudioUpload");
    sendMixPanelEvent(MIX_PANEL_EVENT_SPEAK,{"status":"AudioUploadEnd"});
    //sendDeviceEvents(mixPanelEvent(MIX_PANEL_EVENT_SPEAK,{"status":"AudioUploadEnd"}));
    // If  no audio data, just exit
    if (gAudioString.len() == 0)
    {
        agentLog("No audio data to send to server.");
        return;
    }

    if (gChunkCount != data.scansize)
    {
        agentLog(format("ERROR: expected %d chunks, got %d", 
                   data.scansize, gChunkCount));
    }

    if (data.scandata != "")
    {
        agentLog("Error: found barcode when expected only audio data");
        gAudioState = AudioStates.AudioError;
    sendMixPanelEvent(MIX_PANEL_EVENT_SPEAK,{"status":"AudioUploadEndFail","barcode":data.scandata});
    //sendDeviceEvents(mixPanelEvent(MIX_PANEL_EVENT_SPEAK,{"status":"AudioUploadEndFail","barcode":data.scandata}));
    }

    local sendToDebugServer = false;
    //sendToDebugServer = true;
    // HACK
    // HACK 
    // HACK
    // needs to be rewritten as gAudioBuffer has been replaced with
    // gAudioString
    /*
    if (sendToDebugServer)
    {
        // Send audio to server
        agentLog(format("Audio ready to send. Size=%d", gAudioBuffer.len()));
        local req = http.post("http://bobert.net:4444/"+nv.gImpeeId, 
                             {"Content-Type": "application/octet-stream"}, 
                             http.base64encode(gAudioBuffer));
        local res = req.sendsync();

        if (res.statuscode != 200)
        {
            agentLog("An error occurred:");
            agentLog(format("statuscode=%d", res.statuscode));
            device.send("uploadCompleted", "failure");
        }
        else
        {
            agentLog("Audio sent to server.");
            device.send("uploadCompleted", "success-server");
        }

        return;
    } */
    gEndUploadData = data;
    waitUntilTokenReceived();
});

// Use a scheduler to upload the end of audio as soon as we receive the token
function waitUntilTokenReceived()
{
    server.log(gAudioToken);
    server.log(gEndUploadData);
    if (gAudioToken && gAudioToken.len() > 0)
    {
        sendBeepToHikuServer(gEndUploadData);
        gEndUploadData = null;
    }
    else
    {
        server.log("Audio Token is not yet received!! scheduling to try again!!!!!!!");
        //imp.wakeup(0.150, waitUntilTokenReceived);
    }
}

//**********************************************************************
// Handle an audio buffer from the device
device.on("uploadAudioChunk", function(data) {
    //agentLog(format("in device.on uploadAudioChunk"));
    //agentLog(format("chunk length=%d", data.length));
    dumpBlob(data.buffer);
    sendMixPanelEvent(MIX_PANEL_EVENT_SPEAK,{"status":"AudioChunkReceivedFromDevice"});
    //sendDeviceEvents(mixPanelEvent(MIX_PANEL_EVENT_SPEAK,{"status":"AudioChunkReceivedFromDevice"}));
    // Add the new data to the audio buffer, truncating if necessary
    data.buffer.resize(data.length);  // Most efficient way to truncate? 
    gAudioString=gAudioString+data.buffer.tostring();
    gChunkCount++;
});

device.on("shutdownRequestReason", function(status){
    agentLog(format("Hiku shutting down. Reason=%d", status));
});


device.on("deviceReady", function(data)
{
   nv.deviceReady = time(); 
});

//======================================================================
// External HTTP request handling

//**********************************************************************
// Handle incoming requests to my external agent URL.  

http.onrequest(function (req, res)
{
    // Handle supported requests
    try {
        server.log(format("Request Path: %s",req.path));
      if ( req.method == "GET") { 
        if (req.path == "/getImpeeId") 
        {
          res.send(200, nv.gImpeeId);
        }
        else if (req.path == "/deviceReady")
        {
            local result = http.urlencode({"ready":"false","time":""});
            if (nv.deviceReady != null)
            {
                local c = time();
                local d = nv.deviceReady;
                if (c - d <= 15)
                {
                    result = http.urlencode({"ready":"true", "time":getUTCTimeFromDate(date(d))});
                    //return res.send(200,result);
                }
            }
            return res.send(200, result);
        }
        else if (req.path == "/shippingMode") 
        {
          // HACK
          // safer implementation should require the device to call back the 
          // backend confirming that it is entering shipping mode
          device.send("shippingMode",1);
          res.send(200, "OK");
        } 
        else if (req.path == "/devicePage") 
        {
          //device.send("devicePage",1);
            res.send(200, "OK");
        } 
        else if( req.path == "/getAgentVersion" )
        {   
            res.send(200,gAgentVersion);
        }
        else if( req.path == "/audio/" + gAudioToken)
        {
          local audioLen = gAudioString.len();
          local audioSubstring;

          if (gAudioReadPointer < audioLen) {
             audioSubstring = gAudioString.slice(gAudioReadPointer, audioLen);
             gAudioReadPointer = audioLen;
          } else {
             audioSubstring = "";
          } 
          //agentLog(format("AUDIOPOINTER %d len %d", gAudioReadPointer, audioLen));
    
            res.header("Content-Type", "audio/x-wav");
            res.header("audioState", gAudioState);

            res.send(200, audioSubstring);
        }
        else if (req.path == "/erase" || req.path == "/erase/") {
            res.header("Access-Control-Allow-Origin", "*");
            res.header("Access-Control-Allow-Headers","Origin, X-Requested-With, Content-Type, Accept");
            res.header("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
            res.send(200, "OK\n");
            device.send("erase_stm32", 0);
        }
        else if (req.path == "/scan_image.pgm") {
            server.log(format("Lines scanned %d", lines_scanned));
            local pgm_img_header = format("P5\n# scan_image.pgm\n%d %d\n255\n", IMAGE_COLUMNS, lines_scanned);
            res.header("Content-Type", "image/x-portable-graymap");
            res.send(200, pgm_img_header+scanned_image);
        }
        //else if( req.path == "/getAudioRecording" )
        //{
        //    res.send(200, gAudioString);
        //} 
        else
        {
          agentLog(format("HTTP GET: Unexpected path %s", req.path));
          res.send(404, format("HTTP GET: Unexpected path %s", req.path));
        }
      } else if ( req.method == "POST") {
        if (req.path == "/push" || req.path == "/push/") {
          server.log("Agent received new firmware, starting update");
          fw_len = req.body.len();
          agent_buffer = blob(fw_len);
          agent_buffer.writestring(req.body);
          agent_buffer.seek(0,'b');
          // determine if this is intel hex or binary (or other)
          get_filetype(agent_buffer[0]);
          // notify device we're ready to start
          device.send("load_fw", fw_len);
          res.send(200, "OK\n");
        }
        else
        {
          agentLog(format("HTTP POST: Unexpected path %s", req.path));
          res.send(404, format("HTTP POST: Unexpected path %s", req.path));
        }      
      } else {
        agentLog(format("HTTP method not allowed: %s", req.method));
        res.send(405, format("HTTP method not allowed: %s", req.method));
      }
    } catch (ex) {
     agentLog("Internal Server Error: " + ex);
     res.send(501, "Internal Server Error: " + ex);
  }
});


function getDisconnectReason(reason)
{
  //NO_WIFI=1, NO_IP_ADDRESS=2, NO_SERVER=4, NOT_RESOLVED=3
    if (reason == 1) {
        return "Wifi went away";
    }
 
    if (reason == 2) {
        return "Failed to get IP address";
    }
 
    if (reason == 4) {
        return "Failed to connect to server";
    }
 
    if (reason == 3) {
        return "Failed to resolve server";
    }
 
    return "No Disconnects"
}

function xlate_bootreason_to_string(boot_reason)
{
    local reason = "";
    local pin = 0;
    for( pin =0; pin < 8; pin ++ )
    {
        if( boot_reason & ( 1 << pin ) )
        {
            reason += boot_reasons[pin];
        }
    }
    if( reason == "")
    {
        reason = "COLDBOOT";
    }
    return reason;
}


function updateImpeeId(data)
{
    nv.gImpeeId = data
    server.log(format("Impee Id got Updated: %s", nv.gImpeeId));
    sendDeviceEvents(
                        {     
                              fw_version=nv.gFwVersion,
                              wakeup_reason = xlate_bootreason_to_string(nv.gWakeUpReason),
                              boot_time = nv.gBootTime,
                              sleep_duration = nv.gSleepDuration,
                              rssi = data.rssi,
                        }
                    );  
}


//**********************************************************************
// Receive impee ID from the device and send to the external requestor 
device.on("init_status", function(data) {
    nv.gImpeeId = data.impeeId;
    nv.gFwVersion = data.fw_version;
    nv.gWakeUpReason = data.bootup_reason;
    nv.gSleepDuration = data.sleep_duration;
    nv.at_factory = data.at_factory;
    nv.macAddress = data.macAddress;
    nv.countryCode = data.countryCode;
    
    //server.log(format("Device to Agent Time: %dms", (time()*1000 - data.time_stamp)));
    server.log(format("Device OS Version: %s", data.osVersion));
    server.log(format("Device Country: %s", nv.countryCode));
    local dataToSend =  {     
              fw_version=nv.gFwVersion,
              wakeup_reason = xlate_bootreason_to_string(nv.gWakeUpReason),
              boot_time = nv.gBootTime,
              sleep_duration = nv.gSleepDuration,
              rssi = data.rssi,
              dc_reason = getDisconnectReason(data.disconnect_reason),
              os_version = data.osVersion,
              connectTime = data.time_to_connect,
              ssid = data.ssid,
    };
    sendDeviceEvents(dataToSend);
    sendMixPanelEvent(MIX_PANEL_EVENT_STATUS,dataToSend);
    //sendDeviceEvents(mixPanelEvent(MIX_PANEL_EVENT_STATUS,dataToSend));
    
});

// Receive the Charger state update from the device to be used to send to the
// external server
// @param: chargerState of True means Charger is attached, false otherwise
device.on("chargerState", function( chargerState ){
    nv.gChargerState = chargerState;
    local dataToSend = {      
               charger_state = nv.gChargerState?"charging":"not charging",
             }
    sendDeviceEvents(dataToSend);
    sendMixPanelEvent(MIX_PANEL_EVENT_CHARGER,dataToSend);
    //sendDeviceEvents(mixPanelEvent(MIX_PANEL_EVENT_CHARGER,dataToSend));
});


//======================================================================
// Utility Functions



function getUTCTime()
{
    //[dateFormatter setDateFormat:@"yyyy-MM-dd HH:mm:ss.SSSSSS"];
    local d=date();
    return getUTCTimeFromDate(d);
}

function getUTCTimeFromDate(d)
{
    local str ="";
    str = format("%04d-%02d-%02d %02d:%02d:%02d.000000", d.year, d.month+1, d.day, d.hour, d.min, d.sec);
    return str;    
}

function BlobToHexString(data) {
  local str = "";
  foreach (b in data) str += format("%02x", b);
  return str;
}

//**********************************************************************
// Print all ServerRequest fields 
function logServerRequest(request)
{
    agentLog(format("request.body: \"%s\"", request.body));
    agentLog(format("request.headers:"));
    foreach (key, value in request.headers)
    {
        agentLog(format("-->   \"%s=%s\"", key, value));
    }
    agentLog(format("request.method: \"%s\"", request.method));
}


//**********************************************************************
// Proxy for server.log that prints a line prefix showing it is from the agent
function agentLog(str)
{
    sendLogToServer(format("%s AGENT: %s", getUTCTime(), str));
    server.log(format("AGENT: %s", str));
}


//**********************************************************************
// Print the contents of a table
function dumpTable(data, prefix="")
{
    foreach (k, v in data)
    {
        if (typeof v == "table")
        {
            server.log(prefix + k.tostring() + " {");
            dumpTable(v, prefix+"-");
            server.log(prefix + "}");
        }
        else
        {
            if (v == null)
            {
                v = "(null)"
            }
            server.log(prefix + k.tostring() + "=" + v.tostring());
        }
    }
}


//**********************************************************************
// Print the contents of a blob to the log in a formatted way
function dumpBlob(data)
{
    // Constants
    const cBlob8BitSigned = 'c';
    const cBlob16BitSigned = 's';
    const cBlob16BitUnsigned = 'w';

    // Output parameters
    local dataType = cBlob8BitSigned; // Type of data stored (see
                                      // blob documentation)
    local elementsPerLine = 12; // Number of elements to print per line
    local linesToDump = 10; // Max number of lines to dump

    local str = ""; 
    local elements = 0;
    local lines = 0;

    data.seek(0);
    while(!data.eos())
    {
        // If too many lines, indicate there is more data not printed,
        // drop any current data, and exit the loop
        if (lines > linesToDump)
        {
            server.log("(truncated...)");
            elements = 0; 
            break;
        }

        // Get the next element
        str += data.readn(dataType) + " ";
        elements++;

        // If we have a full line, print it out 
        if (elements >= elementsPerLine)
        {
            server.log(str);
            str = "";
            elements = 0;
            lines++;
        }
    }
    if (elements > 0)
    {
        // Got to end of buffer with less than a full line. Print remainder. 
        server.log(str);
    }
}

//**********************************************************************
// Receive and send out the beep packet
device.on("testBarcode", function(byte_string) {
  local num_string = "[";
  local i;
    
  for (i=0; i<byte_string.len()-1; i++)
            num_string += format("0x%02x,", byte_string[i]);
  num_string += format("0x%02x]", byte_string[i]);
  local data = http.urlencode({"byte_string": byte_string});
  local req = http.post("http://54.148.135.101",
            {"Content-Type": "application/x-www-form-urlencoded", 
            "Accept": "application/json"}, 
            data);
  req.sendsync();
  server.log(num_string); 
});


// STM32 microprocessor firmware updater agent
// Copyright (c) 2014 Electric Imp
// This file is licensed under the MIT License
// http://opensource.org/licenses/MIT

// GLOBALS AND CONSTS ----------------------------------------------------------
const BLOCKSIZE = 4096; // size of binary image chunks to send down to device
const DELAY_DONE_NOTIFICATION_BY = 0.1; // pause to let the device write the final chunk before ending download
const INTELHEX_CHAR = 0x3A; // opening character of an intel hex file

enum filetypes {
    INTELHEX,
    BIN
}

agent_buffer <- blob(2);
hex_buffer <- "";
bin_buffer <- blob(2);
fetch_url <- "";
fw_ptr <- 0;
fw_len <- 0;
filetype <- null;
bin_ptr <- 0;
bin_len <- 0;
bytes_sent <- 0;

// FUNCTION AND CLASS DEFINITIONS ----------------------------------------------

// Helper: Parses a hex string and turns it into an integer
// Input: hexidecimal number as a string
// Return: number (integer)
function hextoint(str) {
    local hex = 0x0000;
    foreach (ch in str) {
        local nibble;
        if (ch >= '0' && ch <= '9') {
            nibble = (ch - '0');
        } else {
            nibble = (ch - 'A' + 10);
        }
        hex = (hex << 4) + nibble;
    }
    return hex;
}

// Helper: Compute the 1-byte modular sum of a line of Intel Hex
// Input: Line of Intel Hex (string)
// Return: modular sum (integer, 1-byte)
function modularsum8(line) {
    local sum = 0x00
    for (local i = 0; i < line.len(); i+=2) {
        sum += hextoint(line.slice(i,i + 2));
    }
    return ((~sum + 1) & 0xff);
}

// Helper: Compute the 32-bit modular sum of a blob
// Input: data (blob)
// Return: modular sum (integer)
function modularsum32(data) {
    local pos = data.tell()
    data.seek(0,'b');
    local sum = 0x00000000
    for (local i = 0; i < line.len(); i+=4) {
        sum += data.readn('i');
    }
    data.seek(pos,'b');
    return ((~sum + 1) & 0xffffffff);
}

// Helper: Parse a buffer of hex into the bin_buffer
// Input: Intel Hex data (string)
// Return: None
//      (writes binary into bin_buffer)
function parse_hexfile(hex) {
    try {
        // line up are start at the point in the bin buffer we've already parsed up to
        bin_buffer.seek(bin_len);
        // make sure we have enough bin buffer to write in all the things we're going to parse
        // for (local i = bin_len; i < BLOCKSIZE; i++) bin_buffer.writen(0xFF, 'b');
        // bin_buffer.seek(bin_len);
        
        local from = 0, to = 0, line = "", offset = 0x00000000;
        do {
            if (to < 0 || to == null || to >= hex.len()) break;
            from = hex.find(":", to);
            
            if (from < 0 || from == null || from + 1 >= hex.len()) break;
            to = hex.find(":", from + 1);
            
            if (to < 0 || to == null || from >= to || to >= hex.len()) break;
            // make sure to strip nasty trailing \r\n
            line = rstrip(hex.slice(from + 1, to));
            //server.log(format("[%d,%d] => %s", from, to, line));
            
            if (line.len() > 10) {
                local len = hextoint(line.slice(0, 2));
                local addr = hextoint(line.slice(2, 6));
                local type = hextoint(line.slice(6, 8));
 
                // Ignore all record types except 00, which is a data record. 
                // Look out for 02 records which set the high order byte of the address space
                if (type == 0) {
                    // Normal data record
                //} else if (type == 4 && len == 2 && addr == 0 && line.len() > 12) {
                } else if (type == 4) {
                    //server.log(format("Type 4 Line, len %d, addr %08x, line len %d", len, addr, line.len()));
                    // Set the offset
                    offset = hextoint(line.slice(8, 12)); // << 16;
                    if (offset != 0) {
                        //server.log(format("Set offset to 0x%08X", offset));
                        //server.log(format("From: %d, To: %d",from,to));
                        // right now, we ignore offset changes and assume the full images will be contiguous!
                    }
                    continue;
                } else {
                    //server.log("Skipped: " + line)
                    continue;
                }
 
                // Read the data from 8 to the end (less the last checksum byte)
                for (local i = 8; i < (8 + (len * 2)); i += 2) {
                    local datum = hextoint(line.slice(i, i + 2));
                    bin_buffer.writen(datum, 'b')
                }
                
                // Checking the checksum would be a good idea but skipped for now
                local read = hextoint(line.slice(line.len() - 2, line.len()));
                local calc = modularsum8(line.slice(0,line.len() - 2));
                if (read != calc) {
                    throw format("Hex File Checksum Error: %02x (read) != %02x (calc) [%s]", read, calc, line);
                }
            }
        } while (from != null && to != null && from < to);
        
        // Resize the raw hex buffer so that it starts at the next line we need to parse
        //server.log(format("Resizing Hex Buffer [%d to %d]",from,hex_buffer.len()));
        hex_buffer = hex_buffer.slice(from, hex_buffer.len());
        
        bin_len += (bin_buffer.tell() - bin_len);
        bin_buffer.seek(bin_ptr);
        //server.log(format("Done parsing chunk, %d bytes in bin buffer",bin_len));
        
        //server.log("Free RAM: " + (imp.getmemoryfree()/1024) + " kb")
        return true;
        
    } catch (e) {
        server.log(e)
        return false;
    }
}

// Helper: take in either a single char or a url string and use it to determine the source file type
// Input: [optional] byte
//      If a byte (first char of file) is provided, will attempt to match char to determine filetype
//      Otherwise, will attempt to fetch a char from fetch_url
// Return: None
//      filetype is set as a side-effect
function get_filetype(byte = null) {
    local char = ""
    if (byte) { 
        char = byte;
    } else if (fetch_url != "") {
        char = http.get(fetch_url, { Range="bytes=0-1" }).sendsync().body[0];
    } else {
        throw "Unable to determine filetype: no args given and fetch_url not set"
    }
    
    if (char == INTELHEX_CHAR) {
        server.log("Filetype: Intel Hex");
        filetype = filetypes.INTELHEX;
    } else {
        server.log("Filetype: Binary");
        filetype = filetypes.BIN;
    }
}

// Send a message to the device to end the device firmware update process
// This function then resets all global variables used in firmware update
// Input: None
// Return: None
function finish_dl() {
    device.send("dl_complete",0);
    server.log("Download complete");
    fetch_url = "";
    fw_ptr = 0;
    fw_len = 0;
    filetype = null;
    // reclaim memory
    hex_buffer = "";
    agent_buffer = blob(2);
    // intel hex parsing may not have been used, but clean up in case
    bin_ptr = 0;
    bin_len = 0;
    bin_buffer = blob(2);
    bytes_sent = 0;
}

// "pull" events from the device are routed here if the source file is in Intel Hex format
// This function responds to the "pull" event with a "push" event and a block of raw
// binary image data, of size BLOCKSIZE
// If this function determines that it has sent the end of the new image, it will trigger
// finish_dl() to end the process
function send_from_intelhex(dummy = 0) {
    if (bin_len > BLOCKSIZE) {
        bin_buffer.seek(bin_ptr,'b');
        // we have more than a full chunk of raw binary data to send to the device, so send it.
        device.send("push", bin_buffer.readblob(BLOCKSIZE));
        bytes_sent += BLOCKSIZE;
        // resize our local buffer of parsed data to contain only unsent data
        local parsed_bytes_left = bin_len - bin_buffer.tell();
        //server.log(format("Sent %d bytes, %d bytes remain in bin_buffer",bin_buffer.tell(),parsed_bytes_left));
        // don't need to seek, as we've just read up to the end of the chunk we sent
        local swap = bin_buffer.readblob(parsed_bytes_left);
        bin_buffer.resize(parsed_bytes_left);
        bin_buffer.seek(0,'b');
        bin_buffer.writeblob(swap);
        bin_buffer.seek(0,'b');
        bin_ptr = 0;
        bin_len = parsed_bytes_left;
        server.log(format("FW Update: Parsed %d/%d bytes, sent %d bytes",fw_ptr,fw_len,bytes_sent));
    } else {
        if (fw_ptr == fw_len) {
            if (bin_ptr == bin_len) {
                // We've already sent the last (partial) chunk; finishe the download
                finish_dl();
            } else {
                // there's nothing left to fetch on the server we're fetching from
                // just send what we have
                device.send("push", bin_buffer);
                bytes_sent += bin_buffer.len();
                server.log(format("FW Update: Parsed %d/%d bytes, sent %d bytes (Final block)",fw_ptr,fw_len,bytes_sent));
                bin_ptr = bin_len;
            }
        } else {
            // fetch more data from the remote server and parse it, then come back here to send it
            local bytes_left_remote = fw_len - fw_ptr;
            local buffersize = bytes_left_remote > BLOCKSIZE ? BLOCKSIZE : bytes_left_remote;
            if (fetch_url != "") {
                // we're fetching from remote
                hex_buffer += http.get(fetch_url, { Range=format("bytes=%u-%u", fw_ptr, fw_ptr + buffersize - 1) }).sendsync().body;
            } else {
                // we're reading from local blob
                hex_buffer += agent_buffer.readblob(buffersize).tostring();
            }
            fw_ptr += buffersize;
            // this will parse the string right into bin_buffer
            parse_hexfile(hex_buffer);
            // go around again!
            send_from_intelhex();
        }
    }
}

// "pull" events from the device are routed here if the source file is in raw binary format
// This function responds to the "pull" event with a "push" event and a block of raw
// binary image data, of size BLOCKSIZE
// If this function determines that it has sent the end of the new image, it will trigger
// finish_dl() to end the process
function send_from_binary(dummy = 0) {
    local bytes_left_total = fw_len - fw_ptr;
    local buffersize = bytes_left_total > BLOCKSIZE ? BLOCKSIZE : bytes_left_total;
    
    // check and see if we've finished the download
    if (buffersize == 0) {
        finish_dl();
        return;
    } 
    
    local buffer = blob(buffersize);
    // if we're fetching a remote file in chunks, go get another
    if (fetch_url != "") {
        // download still in progress
        buffer.writestring(http.get(fetch_url, { Range=format("bytes=%u-%u", fw_ptr, fw_ptr + buffersize - 1) }).sendsync().body);
    // we're sending chunks of file from memory
    } else {
        buffer.writeblob(agent_buffer.readblob(buffersize));
    }
    
    device.send("push",buffer);
    fw_ptr += buffersize;
    server.log(format("FW Update: Sent %d/%d bytes",fw_ptr,fw_len));
}

// DEVICE CALLBACKS ------------------------------------------------------------

// Allow the device to inform us of its bootloader version and supported commands
// This agent doesn't use this for anything; this method is here as an example
device.on("set_version", function(data) {
    server.log("Device Bootloader Version: "+data.bootloader_version);
    local supported_cmds_str = ""; 
    foreach (cmd in data.supported_cmds) {
        supported_cmds_str += format("%02x ",cmd);
    }
    server.log("Bootloader supports commands: " + supported_cmds_str);
});

// Allow the device to inform the agent of its product ID (PID)
// This agent doesn't use this for anything; this method is here as an example
device.on("set_id", function(id) {
    // use the GET_ID command to get the PID
    server.log("STM32 PID: "+id);
});

// Serve a buffer of new image data to the device upon request
device.on("pull", function(dummy) {
    if (filetype == filetypes.INTELHEX) {
        send_from_intelhex();
    } else {
        send_from_binary();
    }
});

device.on("scan_start", function (data) {
   scanned_image = "";
   lines_scanned = 0;
});

device.on("scan_line", function (data) {
   scanned_image += data.tostring();
   lines_scanned++;
});

// HTTP REQUEST HANDLER --------------------------------------------------------

//http.onrequest(function(req, res) {
//
//});
    
// MAIN ------------------------------------------------------------------------

server.log("Agent Started. Free Memory: "+imp.getmemoryfree());

// in case both device and agent just booted, give device a moment to initialize, then get info
/*
imp.wakeup(0.5, function() {
    device.send("get_version",0);
    device.send("get_id",0);
});
*/