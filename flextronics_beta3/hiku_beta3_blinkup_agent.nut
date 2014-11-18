const html = @"
<!DOCTYPE html>
<html lang='en'>
    <head>
        <meta charset='utf-8'>
        <meta http-equiv='X-UA-Compatible' content='IE=edge'>
        <meta name='viewport' content='width=device-width, initial-scale=1'>
        <title>BlinkUp</title>
        <link href='https://maxcdn.bootstrapcdn.com/bootstrap/3.3.0/css/bootstrap.min.css' rel='stylesheet'>
    </head>
    <body>
        <div class='container-fluid col-md-4 col-md-offset-4 col-xs-12'>
            <form role='form'>
                <div class='form-group'>
                    <label for='imp_username'>Imp Username</label>
                    <input type='email' class='form-control' id='imp_username' placeholder='Imp username / email'>
                </div>
                <div class='form-group'>
                    <label for='imp_password'>Imp Password</label>
                    <input type='password' class='form-control' id='imp_password' placeholder='Imp Password'>
                </div>
                <div class='form-group'>
                    <label for='wifi_ssid'>Wifi SSID</label>
                    <input type='text' class='form-control' id='wifi_ssid' placeholder='Wifi SSID'>
                </div>
                <div class='form-group'>
                    <label for='wifi_password'>Wifi Password</label>
                    <input type='password' class='form-control' id='wifi_password' placeholder='Wifi Password'>
                </div>
                <div class='form-group'>
                    <label for='btn_command'>Button Sends:</label>
                    <div class='btn-group radio' id='btn_command' style='margin-left: 10px;'>
                        <label class='radio-inline'><input type='radio' name='btn_command' id='blinkup' value='blinkup' checked='checked'> Blinkup</label>                    
                        <label class='radio-inline'><input type='radio' name='btn_command' id='clear' value='clear'> Clear</label>                    
                        <label class='radio-inline'><input type='radio' name='btn_command' id='force' value='force'> Force</label>                    
                    </div>                
                </div>                
                <div class='form-group text-center'>
                    <button id='configure' class='btn btn-success'>Configure</button>
                </div>
            </form>
        </div>
        <script src='https://ajax.googleapis.com/ajax/libs/jquery/1.11.1/jquery.min.js'></script>
        <script src='https://maxcdn.bootstrapcdn.com/bootstrap/3.3.0/js/bootstrap.min.js'></script>
        <script>
            $('#configure').click(function() {
                var data = {
                    imp_username:  $('#imp_username').val(),
                    imp_password:  $('#imp_password').val(),
                    wifi_ssid:     $('#wifi_ssid').val(),
                    wifi_password: $('#wifi_password').val(),
                    btn_command:   $('input[name=btn_command]:checked').val().toLowerCase(),
                };
                $.post(window.location.href, data).
                    done(function(r) {
                        alert('Success: ' + r);
                    }).
                    fail(function(e) {
                        alert('Failed: ' + e.responseText + ' (' + e.status + ' - ' + e.statusText + ')');
                    });
                return false;
            })
        </script>
    </body>
</html>
";
 
function get_tokens(username, password) {
	local url = "https://api.electricimp.com/enrol";
	local headers = { "Content-Type": "application/x-www-form-urlencoded" };
	local body = { "email": username, "password": password };
	local response = http.post(url, headers, http.urlencode(body)).sendsync();
	if (response.statuscode != 200) return false;
 
	local enrol = http.jsondecode(response.body);
	return { "siteid": enrol.siteids[0], "enrol": enrol.token };
}
 
function send_config(with_command=false) {
    if ("ssid" in tokens && "password" in tokens && "siteid" in tokens && "enrol" in tokens) {
        
        if (typeof with_command == "string") tokens.command <- with_command;
        else if (with_command == true) tokens.command <- "blinkup";
        else tokens.command <- "";
        
        device.send("configure", tokens);
    } else {
        server.log("Not configured yet: " + http.jsonencode(tokens));
    }
}
 
 
function web_server(req, res) {
    if (req.method == "GET") {
        if (req.path.find("/clear") != null) {
            device.send("clear", true);
            res.send(200, "OK");
        } else if (req.path.find("/blinkup") != null) {
            device.send("blinkup", true);
            res.send(200, "OK");
        } else {
            res.send(200, html);
        }
    } else if (req.method == "POST") {
        local data = http.urldecode(req.body);
        if ("imp_username" in data && "imp_password" in data && "wifi_ssid" in data && "wifi_password" in data && "btn_command" in data) {
            local newtokens = get_tokens(data.imp_username, data.imp_password);
            if (newtokens) {
                res.send(200, "OK");
                
                tokens.siteid <- newtokens.siteid;
                tokens.enrol <- newtokens.enrol;
                tokens.ssid <- data.wifi_ssid;
                tokens.password <- data.wifi_password;
                tokens.button <- data.btn_command;
                server.save(tokens);
                
                send_config();
            } else {
                res.send(403, "Access denied");
            }
        } else if ("siteid" in data && "enrol" in data && "wifi_ssid" in data && "wifi_password" in data) {
                res.send(200, "OK");
                
                tokens.siteid <- data.siteid;
                tokens.enrol <- data.enrol;
                tokens.ssid <- data.wifi_ssid;
                tokens.password <- data.wifi_password;
                server.save(tokens);
                send_config();
        } else {
            res.send(400, "Missing data");
        }
    }
}
 
 
 
 
tokens <- server.load();
http.onrequest(web_server);
device.on("configure", send_config);
send_config();