$jpk=0; while (<>) { (/jpk/) && ($jpk=1); }
if ($jpk) { print "This file is jeeperz-approved.\n"; }
