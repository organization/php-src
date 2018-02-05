--TEST--
Ensure the ReflectionClass constructor triggers autoload.
--FILE--
<?php
spl_autoload_register(function ($name) {
  echo "In autoload: ";
  var_dump($name);
});

try {
  new ReflectionClass("UndefC");
}
catch (ReflectionException $e) {
  echo $e->getMessage();
}
?>
--EXPECTF--
In autoload: string(6) "UndefC"
Class UndefC does not exist
