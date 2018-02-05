--TEST--
Test strnatcmp() function : left align, whitespace, digits
--CREDITS--
Sol Richardson <sr5732358@hotmail.com>
--FILE--
<?php
/* Prototype  : int strnatcmp  ( string $str1  , string $str2  )
* Description: String comparisons using a "natural order" algorithm
* Source code: ext/standard/string.c
*/

echo "-- Testing strnatcmp() function whitespace, left-align, digit --\n";
echo "-- Leading whitespace, digits, string 1 longer --\n";
$str1 = " 00";
$str2 = " 0";
var_dump( strnatcmp( $str1, $str2) );

echo "-- Leading whitespace, digits, string 2 longer --\n";
$str1 = " 0";
$str2 = " 00";
var_dump( strnatcmp( $str1, $str2) );
?>

--EXPECTF--
-- Testing strnatcmp() function whitespace, left-align, digit --
-- Leading whitespace, digits, string 1 longer --
int(1)
-- Leading whitespace, digits, string 2 longer --
int(-1)
