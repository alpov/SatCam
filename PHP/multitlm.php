<html><body>

<h2>PSAT-2 TLM stream decoder</h2>

<?php

function check_ascii($s, $len)
{
    $result = true;
    for ($i = 0; $i < $len; $i++) {
	$c = $s[$i];
	if (!(($c >= 'a' && $c <= 'z') || ($c >= 'A' && $c <= 'Z') || ($c >= '0' && $c <= '9') || $c == ' ')) $result = false;
    }
    return $result;
}

function parse_tlm($tlm)
{
    // PSAT-2 C apng eFaaijtkpokoaB aaaa A aokF eEadjluappjxay
    $pos = strpos($tlm, "T-2");
    while ($pos !== false) {
	$line = substr($tlm, $pos+4, 48);
	
	//echo "src: '" . $line . "'\n";
	
	$valid_half = $line[0] >= 'A' && $line[0] <= 'D' && $line[1] == ' ' && $line[6] == ' ' && check_ascii($line, 21);
	$valid_full = $valid_half && $line[21] == ' ' && $line[26] == ' ' && $line[27] >= 'A' && $line[27] <= 'D' && $line[28] == ' ' && $line[33] == ' ' && check_ascii($line, 48);
	$valid_sstv = $line[0] == 'S' && $line[1] == ' ' && $line[6] == ' ' && $line[15] == ' ' && check_ascii($line, 30);
	
	if ($valid_full) ; /* ok */
	else if ($valid_half) $line = substr($line, 0, 21);
	else if ($valid_sstv) $line = substr($line, 0, 30);
	else $line = "";
	
	if ($line != "") echo $line . "\n";
	
	$pos = strpos($tlm, "T-2", $pos+3);
    }
}

if (isset($_POST['tlm'])) $tlm = $_POST['tlm'];
else $tlm = "";

if (isset($_POST['tlm2'])) $tlm2 = $_POST['tlm2'];
else $tlm2 = "";

if (isset($_POST['comment'])) $comment = $_POST['comment'];
else $comment = "";

if ($tlm2 != "") {
    echo "Sending to decoder...<br><pre>";
    $line = strtok($tlm2, "\n");
    $i = 1;
    while ($line !== false) {
	$com = "[" . $i . "] " . $comment;
	$url = "http://www.urel.feec.vutbr.cz/esl/psat2/psat2tlm.php?tlm=" . urlencode("PSAT-2 " . trim($line)) . "&comment=" . urlencode($com);
	echo $url . "\n";
	ob_flush();
	flush();
	$data = file_get_contents($url);
	$line = strtok("\n");
	$i++;
    }
    echo "</pre><br>Done!";
    echo "<br><br><a href=\"multitlm.php\">Back to start</a>";
} else if ($tlm == "") {
?>
<form name="form" action="" method="post">
  <table>
  <tr><td valign="top">Telemetry:</td><td><textarea name="tlm" cols=120 rows=25><?php echo $tlm; ?></textarea><br><br></td></tr>
  </td></tr>
  <tr><td></td><td><input type="submit"></td></tr>
  </table>
</form>
<?php
} else  {
?>
<form name="form" action="" method="post">
  <table>
  <tr><td valign="top">Parsed TLM:</td><td><textarea name="tlm2" cols=120 rows=25><?php parse_tlm($tlm); ?></textarea><br><br></td></tr>
  <tr><td valign="top">Comment:</td><td><input type="text" name="comment" id="comment" value="<?php echo $comment; ?>" size=100><br>
  <small>Please provide any important info, like your callsign, email, SatNOGS reception ID etc.</small><br><br>
  </td></tr>
  <tr><td></td><td><input type="submit"></td></tr>
  </table>
</form>
<?php
}
?>

</body></html>
