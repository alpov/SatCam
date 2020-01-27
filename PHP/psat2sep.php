<html><body>

<h2>PSAT-2 TLM decoder</h2>

Separator OK.

<?php

    $fp = fopen("tlm_psk.txt", "a");
    fprintf($fp, "----- %s -----\n", date('Y-m-d H:i:s'));
    fclose($fp);
    
    $fp = fopen("tbl_psk.txt", "a");
    fprintf($fp, "----- %s -----\n", date('Y-m-d H:i:s'));
    fclose($fp);

    $fp = fopen("tlm_sstv.txt", "a");
    fprintf($fp, "----- %s -----\n", date('Y-m-d H:i:s'));
    fclose($fp);
    
    $fp = fopen("tbl_sstv.txt", "a");
    fprintf($fp, "----- %s -----\n", date('Y-m-d H:i:s'));
    fclose($fp);

?>

</body></html>
