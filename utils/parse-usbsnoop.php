<?

/**********************************************************************
 *  Copyright (C) 2010
 *
 *  Douglas Schilling Landgraf <dougsland@redhat.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  Version: 0.0.1
 *
 *  Description:
 *	This is a quick hack to parse logs from the usbsnoop (usb sniffer)
 *
 *  Settings:
 *	If needed to process a huge log maybe is needed to increase
 *      the php settings.
 *
 *          $ vi /etc/php.ini
 *            memory_limit = xxM
 *
 *  Usage:
 *         $ php ./usb_snoop ./Usbsnoop > output.txt
 *
 *  Example:
 *
 *	   009279: 002309 ms 126080 ms c0 0e a0 00 00 00 01 00 <<< 00
 *         009280: 000007 ms 126087 ms c0 10 a0 00 00 00 01 00 <<< 00
 *         009281: 000005 ms 126092 ms 40 0e a0 00 00 00 01 00 >>> 99
 *         009282: 000107 ms 126199 ms c0 0e a0 00 01 00 01 00 <<< 99
 *         009283: 000015 ms 126214 ms c0 0e a0 00 10 00 01 00 <<< 99
 *         009284: 000004 ms 126218 ms 40 10 a0 00 00 00 01 00 >>> 00
 *         009285: 000004 ms 126222 ms 40 0e a0 00 00 00 01 00 >>> 00
 *
 ***********************************************************************/

function removeNL($var) {

	$newstr = str_replace("\n", "", $var);
	return $newstr;
}

function removeZeros($var) {

	/* Removing 00000000 */
	$remZeros   = strstr($var, ": ");

	/* Removing : */
	$remNewLine = str_replace(": ", "", $remZeros);

	/* Removing \n */
	$newstr = removeNL($remNewLine);

	return $newstr;
}

/* Main */

	$i = 0;
	$j = 0;
	$oldTime   = 0;
	$direction = "";
	$ctrlCmds  = "";


	if ($argc < 2) {
		printf("Usage: $argv[0] usbsnoop-log.txt\n");
		exit;
	}

	$file = fopen($argv[1], "r") or exit("Unable to open file!\n");

	/* Copying to a temp. buffer */
	while(!feof($file))
	{
		$arrayFile[$i] = fgets($file);
		$i++;
	}

	while($j < $i) {

		/* Next position */
		$j = $j + 1;

		if(!isset($arrayFile[$j])) {
			break;
		}

		$str = removeNL($arrayFile[$j]);

		$pieces = explode(" ", $str);
		/* Defining URB */
		if (((strstr($str, "<<<")) || (strstr($str, ">>>"))) ) {
			$URB = $pieces[6] . ":";

		}

		if( $pieces[0] != "--") {
			$t = $pieces[0];
			$timeTotalOp = str_replace("[", "", $t);
			$timeTotalOp = $timeTotalOp . " ms";
		}

		/* Updating current time*/
		$currentTime =  $timeTotalOp;

		$str = removeNL($arrayFile[$j]);

		/* Searching for type to analyze*/

		if (strstr($str, "-- URB_FUNCTION_CONTROL_TRANSFER:")) {

			while (1) {

				/* Next line */
				$j = $j + 1;

				if(!isset($arrayFile[$j])) {
					break;
				}

				/* Setting Direction */
				if (strstr($arrayFile[$j], "TransferFlags")) {
					if(strstr($arrayFile[$j], "USBD_TRANSFER_DIRECTION_OUT")) {
						$direction = ">>>";
					}
					else {
						$direction = "<<<";
					}
				}

				if (strstr($arrayFile[$j], "TransferBufferMDL")) {
					$getValues = 1;

					while(1) {
						/* Next line */
						$j = $j + 1;
						if (strstr($arrayFile[$j], "000000")) {
							$aa = "$arrayFile[$j]\n";
							if($getValues == 1) {
								$cmdV = removeZeros($aa);
							}
							else {
								$cmdV .= " " . removeZeros($aa);
							}
						}
						else {
							break;
						}
						$getValues++;
					}
					$j = $j - $getValues;
				}
				if (strstr($arrayFile[$j], "SetupPacket")) {

					/* To catch the command we increase a line */
					$j = $j + 1;

					if ($oldTime == 0) {
						$diff = 0 . " ms";
					} else {
						$diff = $timeTotalOp - $oldTime . " ms";
					}

					$ctrlCmds = removeZeros($arrayFile[$j]);

					if (isset($cmdVD)) {
						printf ("%06d: %06d ms %06d ms %s %s  %s%s\n",
						       $URB + 0, $diff, $timeTotalOp, $ctrlCmds, $direction, $cmdVD, $cmdV);
						$cmdVD = "";
					}
					else {
						printf ("%06d: %06d ms %06d ms %s %s %s\n",
						       $URB + 0, $diff, $timeTotalOp, $ctrlCmds, $direction, $cmdV);
					}
					$oldTime = $timeTotalOp;
					break;
				}
				if (strstr($arrayFile[$j], "[")) {
					break;
				}
			}

		}

		if (strstr($arrayFile[$j], "-- URB_FUNCTION_VENDOR_DEVICE:")) {

			while(1) {

				/* Next Line */
				$j = $j + 1;

				if (strstr($arrayFile[$j], "TransferBufferMDL")) {

					/* Next Line */
					$j = $j + 1;

					if (strstr($arrayFile[$j], "UrbLink")) {
						break;
					}

					$tmpCMD = "$arrayFile[$j]\n";
					$cmdVD = removeZeros($tmpCMD);
				}

				if (strstr($arrayFile[$j], "[")) {
					break;
				}
			}
		}
	}

	fclose($file)
?>
