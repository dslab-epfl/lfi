#!/usr/bin/php
<?php

function usage($me)
{
	echo $me . " <doc file path>\n";
}

if (count($argv) < 2)
{
	usage($argv[0]);
	exit;
}
$clean_tags_regex = '/<(a|\/a|br|table|\/table|tbody|\/tbody|td|\/td|tr|\/tr|tt|\/tt|span|\/span|i|\/i|col)[^>]*>/';
$regex = '/Function: ([^<]*)<\/h3><pre class="programlisting">([^ \t]*)[^<]*<\/pre><p>[^<]*<\/p>\n(<div[^<]*<\/div>)?/';
$constants = '/(?: |:)(-?[\d]+)\b/';

$contents = @file_get_contents($argv[1]);
$results = array();
$vv = 0;
if (false === $contents)
{
	echo "Invalid file/address provided: {$argv[1]}\n";
} else {
	$contents = preg_replace($clean_tags_regex, '', $contents);
	$r = preg_match_all($regex, $contents, $matches);
	if ($r) {
		for ($i = 0; $i < count($matches[1]); $i++)
		{
			if (!empty($matches[3][$i]) && false === strpos($matches[1][$i], "UCSI"))
			{
				$r = strstr($matches[3][$i], "Returns");
			} else {
				$r = false;
			}
			$return_text = $r;
			// echo $matches[1][$i] . " " . $matches[2][$i] . "\n";
			if ($r)
			{
				if (false == strpos($r, "a positive error code"))
				{
				$r = preg_match_all($constants, $r, $matches2);
				if ($r)
				{
					for ($j = 0; $j < count($matches2[1]); $j++)
					{
						// echo $matches2[1][$j] . ", ";
						$results[$matches[1][$i]][] = $matches2[1][$j];
					}
					if (1 == count($matches2[1]) && (false !== strpos($matches[1][$i], 'Has') || false !== strpos($matches[1][$i], 'Is')))
					{
						// echo "0";
						$results[$matches[1][$i]][] = 0;
					}
				} else {
					if (substr($matches[2][$i], -3) == "Ptr")
					{
						// echo "NULL";
						$results[$matches[1][$i]][] = "NULL";
					}
					if (false !== strpos($return_text, "negative value on fail"))
					{
						$results[$matches[1][$i]][] = "-1";
					}
				}
			    }
			} else {
				if ('void' == $matches[2][$i])
				{
					$vv++;
				}
				else
				{
					// shouldn't happen
				}
			}
		}
	}
	echo '#include "std_errors_head.h"' . "\n";
	
	foreach ($results as $fn => $values)
	{
		echo "int $fn" . "_man_errors[] = {\n";
		foreach ($values as $value)
		{
			echo $value . ",";
		}
		echo "12345};\n";
	}
	
	echo "\n\nstruct sys_errors errors_man[] = {";
	foreach ($results as $fn => $values)
	{
		echo '{ "' . $fn . '", ' . $fn . "_man_errors },\n";
	}
	echo "};\n";
	echo "hahh $vv"; 
}

?>
