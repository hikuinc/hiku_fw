var http = require("http");
var fs = require('fs');
eval(fs.readFileSync('curve255.js')+'');
eval(fs.readFileSync('sha512.js')+'');
eval(fs.readFileSync('AES.js')+'');

var padding8 = "00000000";
pin="";
/* Following three APIs are used for random number generation */
function randomUInt32() {
        return Math.floor((Math.random() * 4294967295));
}

function randomUInt16() {
        return Math.floor((Math.random() * 65536));
}

function randomUInt8() {
        return Math.floor((Math.random() * 256));
}

exports.randomUInt8 = randomUInt8;
exports.randomUInt16 = randomUInt16;
exports.randomUInt32 = randomUInt32;

/* This API is used during "hexadecimal string" conversion
Param [in] : str String to be padded with leading 0s
Param [in] : total string length required after padding 0s
Note: Max pad length could be 8
*/
function pad(str, len)
{
    var temp = "";
    var pad_str = padding8.slice(0, len);
    tmp_len = str.length;
        if (tmp_len != len) {
          temp = pad_str.slice(0, len - tmp_len);
        }
    var new_str = temp + str;
    return new_str;
}
exports.pad = pad;

/* Please refer to the "Developer guide to understand how the below algorithm of
generating a shared secret (i.e AES Encrypt/Decrypt key) works */
exports.generate_shared_secret = function generate_shared_secret(data, callback)
{
      var resp_obj = JSON.parse(data);
      var iv_str = resp_obj.iv;

      var device_pub_key = resp_obj.device_pub_key;
      var encrypted_sig = resp_obj.data;
      var esp_checksum_str = resp_obj.checksum;
      session_id = resp_obj.session_id;

      if (device_pub_key === undefined || encrypted_sig === undefined ||
          esp_checksum_str === undefined || session_id === undefined) {
          console.log("Failure : Bad response");
          callback(1, {eflag:0, msg:"Invalid response for /prov/secure-session"});
          return;
      }

     /* Computing shared secret from device public key and our private key */
      var device_pub_key_array = new Array(16);
      var j = 0;
      for (i = 0; i < 64; i = i + 4) {
        /* Changing the Endianess */
        /* The Curve25519 library used in this reference implementation accepts data as array
        of 16-bit Big-Endian numbers. Therefore, below conversion is required */
        var val  = device_pub_key.slice(i, i + 4).toString(16);
        var val1 = parseInt(device_pub_key.slice(i, i + 4).toString(16));
        val = val.replace(/^(.(..)*)$/, "0$1");
        var a = val.match(/../g);
        a.reverse();
        var s2 = a.join("");

        device_pub_key_array[j] = parseInt(s2, 16);
        j++;
      }

      /* Compute the temperory secret */
      var shared_secret = curve25519(my_private_key, device_pub_key_array);

      var j = 0;
      var shared_secret_array = new Array(8);
      for (i = 0; i < 16; i += 2) {
          var val = shared_secret[i].toString(16);
          var val2 = shared_secret[i + 1].toString(16);
          var result = (val + val2).toString(16);
         /* Changing the Endianess */
         /* The Sha512 library used in this reference implementation accepts data as array
         of 32-bit Big-Endian numbers. Therefore, below conversion is required */
            val = val.replace(/^(.(..)*)$/, "0$1");
            var a = val.match(/../g);
            a.reverse();
            var s2 = a.join("");
            result = s2;

            val2 = val2.replace(/^(.(..)*)$/, "0$1");
            a = val2.match(/../g);
            a.reverse();
            s2 = a.join("");
            result = result + s2 ;

            shared_secret_array[j] = parseInt(result, 16);
            j++;
      }

      /* Computing the Sha512 HASH of temperory secret */
      var shared_secret_hash = sjcl.hash.sha512.hash(shared_secret_array);

      /* Computing Sha512 HASH of pin */
      var pin_hash = sjcl.hash.sha512.hash(pin);

      /* XOR Operation on HASH of "temperory secret" and "pin".
      And, generating shared secret from it */
      var len;
      for (i = 0; i < 4; i++) {
            var temp = "";
            aes_key[i] = (pin_hash[i] ^ shared_secret_hash[i]);
            len = (aes_key[i] >>> 0).toString(16).length;
            if (len != 8) {
                temp = padding8.slice(0, 8 - len);
            }
            var t = temp + (aes_key[i] >>> 0).toString(16);
            aes_key[i] = parseInt(temp + (aes_key[i] >>> 0).toString(16), 16);
      }

    /* Decrypt the "data" received as a part of the /prov/secure-session response */
    /* First retrive "iv" */
      var k = 0;
      var iv = new Array(4);
      for (i = 0; i < 32; i+=8) {
        var temp = "";
        var str = pad(iv_str.slice(i, i + 8), 8);
        iv[k] = parseInt(str, 16);
        k++;
      }
      /* Decrypt the Data */
      var data_array = new Array();
      var data_len = encrypted_sig.length;
      var j = 0;
      for (i = 0; i < data_len; i = i + 8) {
        var val  = encrypted_sig.slice(i, i + 8).toString(16);
        var val1 = parseInt(encrypted_sig.slice(i, i + 8).toString(16), 16);

        if (i + 8 > data_len) {
          /* This is required since this last element should have complete 4 byte data.
          So, adding trailing 0s. For example, ffff -> ffff0000 */
          trim = ((i + 8) - data_len);
          var x = encrypted_sig.slice(i, data_len).toString(16) + padding8.slice(0, trim);
          var val1 = parseInt(x, 16);
        }
        data_array[j] = val1;
        j++;
      }

      /* Decrypt the data */
      cipher = new AES.CTR(aes_key, iv);
      cipher.encrypt(data_array);

      /* Computing checksum and verifying it */
      var data_checksum = sjcl.hash.sha512.hash(data_array);

      j = 0;
      for (i = 0; i < 64; i += 8) {
        var tmp_data = parseInt(esp_checksum_str.slice(i, i + 8), 16)
        if (tmp_data.toString(16) != (data_checksum[j] >>> 0).toString(16))
        {
          console.log("Failure : Checksum Verification Failed");
          callback(1, {eflag:1, msg:"Checksum Verification failed"});
          return;
        }
        j++;
      }

      /*  Checking if the decrypted random number is same as generated random number */
        var test = "";
        for (i = 0; i < (data_len/8); i++) {
          test += pad((data_array[i] >>> 0).toString(16), 8);
        }
        if (test != random_number_str) {
          console.log("Failure : Random number validation failed");
          callback(1, {eflag:1, msg:""});
        }

        console.log("Success : Secure-session-Established");
        callback(null, {eflag:1, msg:""});
}

/* This API is used to encrypt the data and construct a JSON object containing iv,
encrypted data and Sha512 checksum.
Param [in] : data in the string format for encryption
Return value : Returns an object containing "iv", "data" and "checksum"
*/
exports.encrypt_secure_data = function encrypt_secure_data(data) {
var esp = new Object();
var data_str = data;
var enc_data_str = "";
var data_checksum_str = "";
var data_array = new Array();
var data_len = data_str.length;
var iv = new Array(4);
var iv_str = "";
var len = 0, j = 0;

  /* Generate the "iv" to be used for encryption randomly */
 for (i = 0; i < 4; i++) {
      iv[i] = randomUInt32();
  }
  for (i = 0; i < 4; i++) {

      len = iv[i].toString(16).length;
       if (len != 8) {
         iv_str += pad(iv[i].toString(16), 8);
       } else {
         iv_str += iv[i].toString(16);
      }
  }
  esp['iv'] = iv_str;

  /* Encrypt the data using AES-CTR 128-bit using "shared secret" */
  data_array = AES.Codec.strToWords(data_str);
  cipher = new AES.CTR(aes_key, iv);
  var temp = cipher.encrypt(data_array);

  for (i = 0; i < (temp.length); i++) {

      len = (temp[i] >>> 0).toString(16).length;
       if (len != 8) {
         enc_data_str += pad((temp[i] >>> 0).toString(16), 8);
       } else {
         enc_data_str += (temp[i] >>> 0).toString(16);
       }
  }
  esp['data'] = enc_data_str;

  /* Computing checksum and writing it to the ESP */
  esp_checksum = sjcl.hash.sha512.hash(data_str);
  for (i = 0; i < (esp_checksum.length); i++) {

      len = (esp_checksum[i] >>> 0).toString(16).length;
       if (len != 8) {
         data_checksum_str += pad((esp_checksum[i] >>> 0).toString(16), 8);
       } else {
         data_checksum_str += (esp_checksum[i] >>> 0).toString(16);
       }
  }
  esp['checksum'] = data_checksum_str;

  /* return the object */
  return esp;
}


/* This API is used to decrypt and validate the data from the JSON object containing iv,
encrypted data and Sha512 checksum.
Param [in] : json_obj which is a JSON object received from the device
Return value : Returns the decrypted data in a string format
*/
exports.decrypt_secure_data = function decrypt_secure_data(json_obj) {
      var resp_obj = JSON.parse(json_obj);
      var trim = 0;
      var iv_str = resp_obj.iv;
      var data_str = resp_obj.data;
      var esp_checksum_str = resp_obj.checksum;

      if (iv_str === undefined || data_str === undefined || esp_checksum_str === undefined) {
          console.log("Failure : Bad response");
          return null;
      }
      /* Retrive "iv" and convert the hexadecimal string into binary data */
      var k = 0;
      var iv = new Array(4);
      for (i = 0; i < 32; i+=8) {
        var temp = "";
        var str = pad(iv_str.slice(i, i + 8), 8);
        iv[k] = parseInt(str, 16);
        k++;
      }

      /* Retrive "data" and convert the hexadecimal string into binary data */
      var data_array = new Array();
      var data_len = data_str.length;
      var j = 0;
      for (i = 0; i < data_len; i = i + 8) {
        var val  = data_str.slice(i, i + 8).toString(16);
        var val1 = parseInt(data_str.slice(i, i + 8).toString(16), 16);
        if (i + 8 > data_len) {
          /* This is required since last element of array should have complete 4 byte data.
          So, adding trailing 0s */
          trim = ((i + 8) - data_len);
          var x = data_str.slice(i, data_len).toString(16) + padding8.slice(0, trim);
          var val1 = parseInt(x, 16);
        }
        data_array[j] = val1;
        j++;
      }

      /* Decrypt the data using "shared secret" */
      cipher = new AES.CTR(aes_key, iv);
      cipher.encrypt(data_array);
      var test = "";
      for (i = 0; i < (data_len/8); i++) {
         test += pad((data_array[i] >>> 0).toString(16), 8);
      }
      var buf = new Buffer(test, 'hex');
      /* This is required since the last few bytes in the decrypted string may be invalid.
      So, need to be trimmed */
      var new_buf = buf.toString().slice(0, (buf.toString().length - trim/2));

      /* Computing checksum and verifying it */
      var data_checksum = sjcl.hash.sha512.hash(new_buf.toString());

      j = 0;
      for (i = 0; i < 64; i += 8) {
        var tmp_data = parseInt(esp_checksum_str.slice(i, i + 8), 16)
        if (tmp_data.toString(16) != (data_checksum[j] >>> 0).toString(16))
        {
          console.log("Failure : Checksum Verification Failed");
          return null;
        }
        j++;
      }

      return new_buf.toString();
  }
