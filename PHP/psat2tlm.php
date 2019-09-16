<html><body>

<h2>PSAT-2 TLM decoder</h2>

<?php

setlocale(LC_NUMERIC, 'cs_CZ');

if (isset($_GET['tlm'])) $tlm = $_GET['tlm'];
else $tlm = "";

if (isset($_GET['comment'])) $comment = $_GET['comment'];
else $comment = "";

?>

<form name="form" action="" method="get">
  <table>
  <tr><td valign="top">Examples:</td><td>PSAT-2 C apng eFaaijtkpokoaB aaaa A aokF eEadjluappjxay<br>
  PSAT-2 S ashd aDbiaaaa qralaitkboFxaa</medium></td></tr>
  <tr><td valign="top">Telemetry:</td><td><input type="text" name="tlm" id="tlm" value="<?php echo $tlm; ?>" size=80><br><br></td></tr>
  <tr><td valign="top">Comment:</td><td><input type="text" name="comment" id="comment" value="<?php echo $comment; ?>" size=50><br>
  <small>Please provide any important info, like your callsign, email, SatNOGS reception ID etc.<br>
  Valid telemetry frames are automatically saved with timestamp and your IP address for further analysis.</small><br><br>
  </td></tr>
  <tr><td></td><td><input type="submit"></td></tr>
  </table>
</form>

<br>

<?php

function getval($inp)
{
    $value = 0;

    $hi5 = ord($inp[0]);
    $lo5 = ord($inp[1]);

    if ($hi5 >= ord('a') && $hi5 <= ord('z')) $value += $hi5 - ord('a');
    else if ($hi5 >= ord('A') && $hi5 <= ord('F')) $value += $hi5 - ord('A') + 26;
    $value <<= 5;

    if ($lo5 >= ord('a') && $lo5 <= ord('z')) $value += $lo5 - ord('a');
    else if ($lo5 >= ord('A') && $lo5 <= ord('F')) $value += $lo5 - ord('A') + 26;
    return $value;
}

function parse_tlm_psk($tlm, $comment)
{
    // PSAT-2 C apng eFaaijtkpokoaB aaaa A aokF eEadjluappjxay
    echo "---- Current frame ----\n\n";

    $tok = strtok($tlm, " ");
    $mode = $tok;
    echo "Mode: " . $tok . "\n";
    echo "\n";

    $tok = strtok(" ");
    $val = (getval(substr($tok, 0, 2)) << 10) + getval(substr($tok, 2, 2));
    echo "ClockTimer = " . $val . " ticks = " . floor($val*20/3600) . gmdate(":i:s", $val*20) . "\n";
    echo "\n";
    $excel = $comment . "\t\t";
    $excel = $excel . $val . "\t" . floor($val*20/3600) . gmdate(":i:s", $val*20) . "\t";
    $excel = $excel . $mode . "\t";

    $tok = strtok(" ");
    echo "RebootCnt  = " . getval(substr($tok, 0, 2)) . " times\n";
    echo "val_PSK    = " . getval(substr($tok, 2, 2)) . " %\n";
    echo "val_AGC    = " . getval(substr($tok, 4, 2)) . "\n";
    echo "val_Vbat   = " . getval(substr($tok, 6, 2)) . " = " . round(getval(substr($tok, 6, 2)) * 3300 * 147 / 47 / 1024 / 1000, 3) . " V\n";
    echo "val_5V     = " . getval(substr($tok, 8, 2)) . " = " . round(getval(substr($tok, 8, 2)) * 2500 * 409 / 100 / 1024 / 1000, 3) . " V\n";
    echo "val_Ic     = " . getval(substr($tok, 10, 2)) . " mA\n";
    $temp = getval(substr($tok, 12, 2));
    if ($temp > 512) $temp = $temp - 1024;
    echo "val_T_RX   = " . strval($temp) . " deg C\n";
    echo "\n";
    $excel = $excel . getval(substr($tok, 0, 2)) . "\t" . getval(substr($tok, 2, 2)) . "\t" . \
      getval(substr($tok, 4, 2)) . "\t" . round(getval(substr($tok, 6, 2)) * 3300 * 147 / 47 / 1024 / 1000, 3) . "\t" . \
      round(getval(substr($tok, 8, 2)) * 2500 * 409 / 100 / 1024 / 1000, 3) . "\t" . getval(substr($tok, 10, 2)) . "\t" . \
      strval($temp) . "\n";

    $tok = strtok(" ");
    $val = getval(substr($tok, 0, 2));
    echo "status.PeriodNr       = " . (($val >> 5) & 0x1F) . "\n";
    echo "status.PeriodsSSTV_RX = " . (($val >> 0) & 0x1F) . "\n";
    $val = getval(substr($tok, 0, 2));
    echo "status.PeriodsRX      = " . (($val >> 5) & 0x1F) . "\n";
    echo "status.PeriodsTX      = " . (($val >> 0) & 0x1F) . "\n";

    echo "\n\n";
    $tok = strtok(" ");
    if ($tok >= 'A' && $tok <= 'D') {
        echo "---- History frame ----\n\n";
    
        $mode = $tok;
        echo "Mode: " . $tok . "\n";
        echo "\n";
    
        $tok = strtok(" ");
        $val = (getval(substr($tok, 0, 2)) << 10) + getval(substr($tok, 2, 2));
        echo "ClockTimer = " . $val . " ticks = " . floor($val*20/3600) . gmdate(":i:s", $val*20) . "\n";
        echo "\n";
        $excel = $excel . $comment . "\t\t";
        $excel = $excel . $val . "\t" . floor($val*20/3600) . gmdate(":i:s", $val*20) . "\t";
        $excel = $excel . $mode . "\t";
    
        $tok = strtok(" ");
        echo "RebootCnt  = " . getval(substr($tok, 0, 2)) . " times\n";
        echo "val_PSK    = " . getval(substr($tok, 2, 2)) . " %\n";
        echo "val_AGC    = " . getval(substr($tok, 4, 2)) . "\n";
        echo "val_Vbat   = " . getval(substr($tok, 6, 2)) . " = " . round(getval(substr($tok, 6, 2)) * 3300 * 147 / 47 / 1024 / 1000, 3) . " V\n";
        echo "val_5V     = " . getval(substr($tok, 8, 2)) . " = " . round(getval(substr($tok, 8, 2)) * 2500 * 409 / 100 / 1024 / 1000, 3) . " V\n";
        echo "val_Ic     = " . getval(substr($tok, 10, 2)) . " mA\n";
        $temp = getval(substr($tok, 12, 2));
        if ($temp > 512) $temp = $temp - 1024;
        echo "val_T_RX   = " . strval($temp) . " deg C\n";
        echo "\n";
        $excel = $excel . getval(substr($tok, 0, 2)) . "\t" . getval(substr($tok, 2, 2)) . "\t" . \
          getval(substr($tok, 4, 2)) . "\t" . round(getval(substr($tok, 6, 2)) * 3300 * 147 / 47 / 1024 / 1000, 3) . "\t" . \
          round(getval(substr($tok, 8, 2)) * 2500 * 409 / 100 / 1024 / 1000, 3) . "\t" . getval(substr($tok, 10, 2)) . "\t" . \
          strval($temp) . "\n";
        
        echo "\n";
    }
    
    echo "---- Spreadsheet format ----\n\n";
    echo $excel;
    echo "\n\n";

    return $excel;
}


function legend_psk()
{
?>

<h3>PSK footnotes</h3>

<b>ClockTimer</b> - frame counter, incremented every 20sec, persistent on reboot

<?php
}


function parse_tlm_sstv($tlm, $comment)
{
    // PSAT-2 S ashd aDbiaaaa qralaitkboFxaa
    echo "---- Current frame ----\n\n";

    $tok = strtok($tlm, " ");
    $mode = $tok;
    echo "Mode: " . $tok . "\n";
    echo "\n";

    $tok = strtok(" ");
    $val = (getval(substr($tok, 0, 2)) << 10) + getval(substr($tok, 2, 2));
    echo "Tick = " . $val . " sec = " . floor($val/3600) . gmdate(":i:s", $val) . "\n";
    echo "\n";
    $excel = $comment . "\t\t";
    $excel = $excel . $val . "\t" . floor($val/3600) . gmdate(":i:s", $val) . "\t";
    $excel = $excel . $mode . "\t";

    $tok = strtok(" ");
    $temp = getval(substr($tok, 0, 2));
    if ($temp > 512) $temp = $temp - 1024;
    echo "ADC_Temperature = " . strval($temp) . " deg C\n";
    $val = getval(substr($tok, 2, 2));
    echo "ADC_Light       = " . (($val%100) * pow(10, floor($val/100))) . " lux\n";
    echo "Plan_Auth       = " . getval(substr($tok, 4, 2)) . "\n";
    echo "Plan_*_Count    = " . getval(substr($tok, 6, 2)) . "\n";
    echo "\n";
    $excel = $excel . strval($temp) . "\t" . (($val%100) * pow(10, floor($val/100))) . "\t" . \
      getval(substr($tok, 4, 2)) . "\t" . getval(substr($tok, 6, 2)) . "\t";

    $tok = strtok(" ");
    echo "cnt_Boot        = " . getval(substr($tok, 0, 2)) . " times\n";
    echo "cnt_*_Error     = " . getval(substr($tok, 2, 2)) . " times\n";
    echo "cnt_AudioStart  = " . getval(substr($tok, 4, 2)) . " times\n";
    echo "cnt_CamSnapshot = " . getval(substr($tok, 6, 2)) . " times\n";
    echo "cnt_CmdHandled  = " . getval(substr($tok, 8, 2)) . " times\n";
    echo "cnt_CmdIgnored  = " . getval(substr($tok, 10, 2)) . " times\n";
    echo "cnt_AuthError   = " . getval(substr($tok, 12, 2)) . " times\n";
    echo "\n";
    $excel = $excel . getval(substr($tok, 0, 2)) . "\t" . getval(substr($tok, 2, 2)) . "\t" . \
      getval(substr($tok, 4, 2)) . "\t" . getval(substr($tok, 6, 2)) . "\t" . \
      getval(substr($tok, 8, 2)) . "\t" . getval(substr($tok, 10, 2)) . "\t" . \
      getval(substr($tok, 12, 2)) . "\n";

    echo "\n";
    echo "---- Spreadsheet format ----\n\n";
    echo $excel;
    echo "\n\n";
    
    return $excel;
}


function legend_sstv()
{
?>

<h3>SSTV footnotes</h3>

<b>Tick</b> - tick counter, i.e. number of seconds elapsed from reboot, not persistent on reboot<br>
<b>Plan_Auth</b> - authorization time counter; counts down the number of seconds, for which is the authorization valid (zero if not authorized)<br>
<b>Plan_*_Count</b> - number of planned events (sum of sstv_live, sstv_save, psk and cw)<br>
<b>cnt_Boot</b> - count of BOOT<br>
<b>cnt_*_Error</b> - sum of HARDFAULT, FLASH_INIT_ERROR, FLASH_TIMEOUT, CAM_I2C_ERROR, CAM_DCMI_ERROR, CAM_SIZE_ERROR, JPEG_ERROR and PSK_TIMEOUT<br>
<b>cnt_AudioStart</b> - count of AUDIO_START (start of SSTV/PSK/CW transmission)<br>
<b>cnt_CamSnapshot</b> - count of CAM_SNAPSHOT (frame transferred via DCMI)<br>
<b>cnt_CmdHandled</b> - sum of CMD_HANDLED and PSK_UPLINK (accepted command from APRS or CW)<br>
<b>cnt_CmdIgnored</b> - count of CMD_IGNORED (rejected data from APRS)<br>
<b>cnt_AuthError</b> - count of AUTH_ERROR (access to restricted command without autorization)<br>

<?php
}


if ($tlm != "") {
    if (strpos($tlm, "T-2 ") === false) {
        echo "No PSAT-2 header";
    } else {
        $tlm2 = substr($tlm, strpos($tlm, " ")+1, 255);
        if ($tlm2[0] == 'A' || $tlm2[0] == 'B' || $tlm2[0] == 'C' || $tlm2[0] == 'D') {
            echo "<h3>PSK TLM</h3>\n";
            echo "<pre>\n";
            $excel = parse_tlm_psk($tlm2, $comment);
            echo "</pre>";
            legend_psk();
            $fp = fopen("tlm_psk.txt", "a");
            fprintf($fp, "%s;%s;%s;%s\n", date('Y-m-d H:i:s'), $_SERVER['REMOTE_ADDR'], $tlm, $comment);
            fclose($fp);
            $fp = fopen("tbl_psk.txt", "a");
            fwrite($fp, $excel);
            fclose($fp);
        }
        else if ($tlm2[0] == 'S') {
            echo "<h3>SSTV TLM</h3>\n";
            echo "<pre>\n";
            $excel = parse_tlm_sstv($tlm2, $comment);
            echo "</pre>";
            legend_sstv();
            $fp = fopen("tlm_sstv.txt", "a");
            fprintf($fp, "%s;%s;%s;%s\n", date('Y-m-d H:i:s'), $_SERVER['REMOTE_ADDR'], $tlm, $comment);
            fclose($fp);
            $fp = fopen("tbl_sstv.txt", "a");
            fwrite($fp, $excel);
            fclose($fp);
        }
        else {
            echo "Unknown TLM type";
        }
    }
}

?>

</body></html>
