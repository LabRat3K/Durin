var mode = 'null';
var wsQueue = [];
var wsBusy = false;
var wsTimerId;
var rfc_cache=0;
var rff_cache=0;


// Default modal properties
$.fn.modal.Constructor.DEFAULTS.backdrop = 'static';
$.fn.modal.Constructor.DEFAULTS.keyboard = false;

var admin_ctl=false;

// jQuery doc ready
$(function() {
    // Menu navigation for single page layout
    $('ul.navbar-nav li a').click(function() {
        // Highlight proper navbar item
        $('.nav li').removeClass('active');
        $(this).parent().addClass('active');

        // Show the proper menu div
        $('.mdiv').addClass('hidden');
        $($(this).attr('href')).removeClass('hidden');

        // kick start the live stream
        if ($(this).attr('href') == "#diag") {
            wsEnqueue('V1');
        }

        // Collapse the menu on smaller screens
        $('#navbar').removeClass('in').attr('aria-expanded', 'false');
        $('.navbar-toggle').attr('aria-expanded', 'false');

        // Firmware selection and upload
        $('#efu').change(function () {
            $('#update').modal();
            $('#updatefw').submit();
        });


        // Set page event feeds
        feed();
    });

    // SLACK field toggles
    $('#slack').click(function() {
        if ($(this).is(':checked')) {
            $('.sledit').removeClass('hidden');
       } else {
            $('.sledit').addClass('hidden');
       }
    });

    // Discord field toggles
    $('#discord').click(function() {
        if ($(this).is(':checked')) {
            $('.dcedit').removeClass('hidden');
       } else {
            $('.dcedit').addClass('hidden');
       }
    });

    // UDP field toggles
    $('#udp').click(function() {
        if ($(this).is(':checked')) {
            $('.uedit').removeClass('hidden');
       } else {
            $('.uedit').addClass('hidden');
       }
    });

    // DHCP field toggles
    $('#dhcp').click(function() {
        if ($(this).is(':checked')) {
            $('.dhcp').addClass('hidden');
       } else {
            $('.dhcp').removeClass('hidden');
       }
    });

    // Hostname, SSID, and Password validation
    $('#hostname').keyup(function() {
        wifiValidation();
    });
    $('#staTimeout').keyup(function() {
        wifiValidation();
    });
    $('#ssid').keyup(function() {
        wifiValidation();
    });
    $('#password').keyup(function() {
        wifiValidation();
    });
    $('#ap').change(function () {
        wifiValidation();
    });
    $('#dhcp').change(function () {
        wifiValidation();
    });
    $('#gateway').keyup(function () {
        wifiValidation();
    });
    $('#ip').keyup(function () {
        wifiValidation();
    });
    $('#netmask').keyup(function () {
        wifiValidation();
    });

    // Config
    $('#dname').keyup(function() {
        configValidation();
    });

    $('#slfp').keyup(function() {
        configValidation();
    });

    $('#slwht').keyup(function() {
        configValidation();
    });

    $('#slwha').keyup(function() {
        configValidation();
    });

    $('#uport').keyup(function() {
        configValidation();
    });

    $('#uip').keyup(function () {
        configValidation();
    });

    // autoload tab based on URL hash
    var hash = window.location.hash;
    hash && $('ul.navbar-nav li a[href="' + hash + '"]').click();

});

function configValidation() {
   var iptest = new RegExp(''
      + /^(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\./.source
      + /(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\./.source
      + /(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\./.source
      + /(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$/.source
      );

  if($('#udp').prop('checked')) {
   if (iptest.test($('#uip').val())) {
      $('#uip').removeClass('has-error');
      $('#uip').addClass('has-success');
      $('#btn_config').prop('disabled', false);
   } else {
      $('#uip').removeClass('has-success');
      $('#uip').addClass('has-error');
      $('#btn_config').prop('disabled', true);
   }
 } else {
   $('#btn_config').prop('disabled',false);
 }
}

function wifiValidation() {
    var WifiSaveDisabled = false;
    var re = /^([a-zA-Z0-9][a-zA-Z0-9][a-zA-Z0-9\-.]*[a-zA-Z0-9.])$/;
    if (re.test($('#hostname').val()) && $('#hostname').val().length <= 255) {
        $('#fg_hostname').removeClass('has-error');
        $('#fg_hostname').addClass('has-success');
    } else {
        $('#fg_hostname').removeClass('has-success');
        $('#fg_hostname').addClass('has-error');
        WifiSaveDisabled = true;
    }
    if ($('#staTimeout').val() >= 5) {
        $('#fg_staTimeout').removeClass('has-error');
        $('#fg_staTimeout').addClass('has-success');
    } else {
        $('#fg_staTimeout').removeClass('has-success');
        $('#fg_staTimeout').addClass('has-error');
        WifiSaveDisabled = true;
    }
    if ($('#ssid').val().length <= 32) {
        $('#fg_ssid').removeClass('has-error');
        $('#fg_ssid').addClass('has-success');
    } else {
        $('#fg_ssid').removeClass('has-success');
        $('#fg_ssid').addClass('has-error');
        WifiSaveDisabled = true;
    }
    if ($('#password').val().length <= 64) {
        $('#fg_password').removeClass('has-error');
        $('#fg_password').addClass('has-success');
    } else {
        $('#fg_password').removeClass('has-success');
        $('#fg_password').addClass('has-error');
        WifiSaveDisabled = true;
    }
    if ($('#dhcp').prop('checked') === false) {
        var iptest = new RegExp(''
        + /^(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\./.source
        + /(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\./.source
        + /(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\./.source
        + /(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$/.source
        );

        if (iptest.test($('#ip').val())) {
            $('#fg_ip').removeClass('has-error');
            $('#fg_ip').addClass('has-success');
        } else {
            $('#fg_ip').removeClass('has-success');
            $('#fg_ip').addClass('has-error');
            WifiSaveDisabled = true;
        }
        if (iptest.test($('#netmask').val())) {
            $('#fg_netmask').removeClass('has-error');
            $('#fg_netmask').addClass('has-success');
        } else {
            $('#fg_netmask').removeClass('has-success');
            $('#fg_netmask').addClass('has-error');
            WifiSaveDisabled = true;
        }
        if (iptest.test($('#gateway').val())) {
            $('#fg_gateway').removeClass('has-error');
            $('#fg_gateway').addClass('has-success');
        } else {
            $('#fg_gateway').removeClass('has-success');
            $('#fg_gateway').addClass('has-error');
            WifiSaveDisabled = true;
        }
    }
    $('#btn_wifi').prop('disabled', WifiSaveDisabled);
}


// Page event feeds
function feed() {
    if ($('#home').is(':visible')) {
        wsEnqueue('XJ');

        setTimeout(function() {
            feed();
        }, 1000);
    }
}

function param(name) {
    return (location.search.split(name + '=')[1] || '').split('&')[0];
}

// WebSockets
function wsConnect() {
    if ('WebSocket' in window) {

// accept ?target=10.0.0.123 to make a WS connection to another device
        if (target = param('target')) {
//
        } else {
            target = document.location.host;
        }

        // Open a new web socket and set the binary type
        ws = new WebSocket('ws://' + target + '/ws');
        ws.binaryType = 'arraybuffer';

        ws.onopen = function() {
            $('#wserror').modal('hide');
            wsEnqueue('E1'); // Get html elements
            wsEnqueue('G1'); // Get Config
            wsEnqueue('G2'); // Get Net Status
            wsEnqueue('G3'); // Get Door Info
            feed();
        };

        ws.onmessage = function (event) {
            if(typeof event.data === "string") {
                var cmd = event.data.substr(0, 2);
                var data = event.data.substr(2);
                switch (cmd) {
                case 'E1':
                    getElements(data);
                    break;
                case 'G1':
                    getConfig(data);
                    break;
                case 'G2':
                    getConfigStatus(data);
                    break;
                case 'G3':
                    getDoorStatus(data);
                    break;
                case 'S1':
                    setConfig(data);
                    reboot();
                case 'S2':
                    setConfig(data);
                    break;
                case 'S3':
                    footermsg('Configuration Saved');
                    break;
                case 'S4':
                    break;
                case 'XJ':
                    getJsonStatus(data);
                    break;
                case 'X6':
                    showReboot();
                    break;
                default:
                    console.log('Unknown Command: ' + event.data);
                    break;
                }
            } else {
                // Placeholder for streaming data content
            }
            wsReadyToSend();
        };

        ws.onclose = function() {
            $('#wserror').modal();
            wsConnect();
        };

    } else {
        alert('WebSockets is NOT supported by your Browser! You will need to upgrade your browser or.');
    }
}

function wsEnqueue(message) {
    //only add a message to the queue if there isn't already one of the same type already queued, otherwise update the message with the latest request.
    wsQueueIndex=wsQueue.findIndex(wsCheckQueue,message);
    if(wsQueueIndex == -1) {
        //add message
        wsQueue.push(message);
    } else {
        //update message
        wsQueue[wsQueueIndex]=message;
    }
    wsProcessQueue();
}

function wsCheckQueue(value) {
    //messages are of the same type if the first two characters match
    return value.substr(0,2)==this.substr(0,2);
}

function wsProcessQueue() {
    //check if currently waiting for a response
    if(wsBusy) {
        //console.log('WS queue busy : ' + wsQueue);
    } else {
        //set wsBusy flag that we are waiting for a response
        wsBusy=true;
        //get next message from queue.
        message=wsQueue.shift();
        //set timeout to clear flag and try next message if response isn't recieved. Short timeout for message types that don't generate a response.
        if(['T0','T1','T2','T3','T4','T5','T6','T7','X6'].indexOf(message.substr(0,2))) {
            timeout=40;
        } else {
            timeout=2000;
        }
        wsTimerId=setTimeout(wsReadyToSend,timeout);
        //send it.
        //console.log('WS sending ' + message);
        ws.send(message);
    }
}

function wsReadyToSend() {
    clearTimeout(wsTimerId);
    wsBusy=false;
    if(wsQueue.length>0) {
        //send next message
        wsProcessQueue();
    } else {
        //console.log('WS queue empty');
    }
}

function getElements(data) {
    var elements = JSON.parse(data);

    for (var i in elements) {
        for (var j in elements[i]) {
            var opt = document.createElement('option');
            opt.text = j;
            opt.value = elements[i][j];
            document.getElementById(i).add(opt);
        }
    }
}

function getConfig(data) {
    var config = JSON.parse(data);
    // Device and Network config
    $('#title').text('DURIN - ' + config.network.hostname);
    $('#name').text(config.device.dname);
    $('#dname').val(config.device.dname);

    $('#s_lampid').val(config.device.lampid);
    $('#lampid').val(config.device.lampid);


    $('#s_vled').val(config.device.vled);
    $('#vled').val(config.device.vled);

    $('#ssid').val(config.network.ssid);
    $('#password').val(config.network.passphrase);
    $('#hostname').val(config.network.hostname);
    $('#staTimeout').val(config.network.sta_timeout);
    $('#dhcp').prop('checked', config.network.dhcp);
    if (config.network.dhcp) {
        $('.dhcp').addClass('hidden');
    } else {
        $('.dhcp').removeClass('hidden');
    }
    $('#ap').prop('checked', config.network.ap_fallback);
    $('#ip').val(config.network.ip[0] + '.' +
            config.network.ip[1] + '.' +
            config.network.ip[2] + '.' +
            config.network.ip[3]);
    $('#netmask').val(config.network.netmask[0] + '.' +
            config.network.netmask[1] + '.' +
            config.network.netmask[2] + '.' +
            config.network.netmask[3]);
    $('#gateway').val(config.network.gateway[0] + '.' +
            config.network.gateway[1] + '.' +
            config.network.gateway[2] + '.' +
            config.network.gateway[3]);


    $('#sltxt').prop('checked', config.slack.txt.enable);
    $('#slwht').val(config.slack.txt.webhook);

    $('#slapi').prop('checked', config.slack.lamp.enable);
    $('#slwha').val(config.slack.lamp.webhook);

    $('#slfp').val(config.slack.fingerprint);

    $('#udp').prop('checked', config.udp.enable);
    if (config.udp.enable) {
        $('.uedit').removeClass('hidden');
    } else {
        $('.uedit').addClass('hidden');
    }
    $('#uip').val(config.udp.uip[0] + '.' +
            config.udp.uip[1] + '.' +
            config.udp.uip[2] + '.' +
            config.udp.uip[3]);

    $('#uport').val(config.udp.port);

    // Output Config
    $('.odiv').addClass('hidden');
}

function getDoorStatus(data) {
    var status = JSON.parse(data);
    console.log('Door Status received: '+status.door_state);
    $('#door_state').text(status.door_state);
}

function getConfigStatus(data) {
    var status = JSON.parse(data);

    $('#x_ssid').text(status.ssid);
    $('#x_hostname').text(status.hostname);
    $('#x_ip').text(status.ip);
    $('#x_mac').text(status.mac);
    $('#x_version').text(status.version);
    $('#x_built').text(status.built);
    $('#x_flashchipid').text(status.flashchipid);
    $('#x_usedflashsize').text(status.usedflashsize);
    $('#x_realflashsize').text(status.realflashsize);
    $('#x_freeheap').text(status.freeheap);

}

function getJsonStatus(data) {
    var status = JSON.parse(data);

    var rssi = +status.system.rssi;
    var quality = 2 * (rssi + 100);

    if (rssi <= -100)
        quality = 0;
    else if (rssi >= -50)
        quality = 100;

    $('#x_rssi').text(rssi);
    $('#x_quality').text(quality);

// getHeap(data)
    $('#x_freeheap').text( status.system.freeheap );

// getUptime
    var date = new Date(+status.system.uptime);
    var str = '';

    str += Math.floor(date.getTime()/86400000) + " days, ";
    str += ("0" + date.getUTCHours()).slice(-2) + ":";
    str += ("0" + date.getUTCMinutes()).slice(-2) + ":";
    str += ("0" + date.getUTCSeconds()).slice(-2);
    $('#x_uptime').text(str);


}

function footermsg(message) {
    // Show footer msg
    var x = document.getElementById('footer');
    var bg = x.style.background;
    var fg = x.style.color;
    var msg = $('#name').text();


    x.style.background='rgb(255,48,48)';
    x.style.color="white";
    $('#name').text(message);

    setTimeout(function(){
        $('#name').text(msg);
        //x.style.background='rgb(14,14,14)';
        x.style.background=bg;
        x.style.color=fg ;
    }, 3000);
}

function snackmsg(message) {
    // Show snackbar for 3sec
    var x = document.getElementById('snackbar');
    x.innerHTML=message;
    x.className = 'show';
    setTimeout(function(){
        x.className = x.className.replace('show', '');
    }, 3000);
}

function setConfig() {
    // Get config to refresh UI and show result
    wsEnqueue("G1");
    snackmsg('Configuration Saved');
}

function door_toggle() {
    wsEnqueue("X1");
    snackmsg('Activating Door');
}

function submitConfig() {
    var udpip = $('#uip').val().split('.');
    var json = {
    	'device': {
             'id':   $('#devid').val(),
             'vled': $('#vled').val(),
             'dname': $('#dname').val()
        },
        'slack': {
            'txt': {
               'enable': $('#sltxt').prop('checked'),
               'webhook'  : $('#slwht').val()
            },
            'lamp': {
               'enable': $('#slapi').prop('checked'),
               'webhook'  : $('#slwha').val()
            },
            'fingerprint':  $('#slfp').val(),
        },
        'udp': {
             'enable': $('#udp').prop('checked'),
             'port':   $('#uport').val(),
             'uip': [parseInt(udpip[0]), parseInt(udpip[1]), parseInt(udpip[2]), parseInt(udpip[3])]
        }
     };
    wsEnqueue('S2' + JSON.stringify(json));
}

function submitWiFi() {
    var ip = $('#ip').val().split('.');
    var netmask = $('#netmask').val().split('.');
    var gateway = $('#gateway').val().split('.');

    var json = {
            'network': {
                'ssid': $('#ssid').val(),
                'passphrase': $('#password').val(),
                'hostname': $('#hostname').val(),
                'sta_timeout': parseInt($('#staTimeout').val()),
                'ip': [parseInt(ip[0]), parseInt(ip[1]), parseInt(ip[2]), parseInt(ip[3])],
                'netmask': [parseInt(netmask[0]), parseInt(netmask[1]), parseInt(netmask[2]), parseInt(netmask[3])],
                'gateway': [parseInt(gateway[0]), parseInt(gateway[1]), parseInt(gateway[2]), parseInt(gateway[3])],
                'dhcp': $('#dhcp').prop('checked'),
                'ap_fallback': $('#ap').prop('checked')
            }
        };

    wsEnqueue('S1' + JSON.stringify(json));
}

function showReboot() {
    $('#update').modal('hide');
    $('#reboot').modal();
    setTimeout(function() {
        if($('#dhcp').prop('checked')) {
            window.location.assign("/");
        } else {
            window.location.assign("http://" + $('#ip').val());
        }
    }, 5000);
}

function reboot() {
    showReboot();
    wsEnqueue('X6');
}

//function getKeyByValue(object, value) {
//    return Object.keys(object).find(key => object[key] === value);
//}

function getKeyByValue(obj, value) {
    return Object.keys(obj)[Object.values(obj).indexOf(value)];
}

