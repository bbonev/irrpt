<?php

function pfxlist_generate($format, $asn, $pfxstr, $pfxstr_v6, $pfxlen, $pfxlen_v6, $o_4, $o_6)
{
	global $cfg;

    if(!check_asn_arg($asn, $asn_number, $asn_string)) {
        status(STATUS_ERROR, "Invalid ASN {$asn}");
        return FALSE;
    }

	// only v4
	if( $o_4 == 1 && $o_6 == 0 )
	{
		$file = $cfg['paths']['db'] . $asn_number . '.4' . $cfg['aggregate']['suffix'];
	}
	// only v6
	elseif( $o_6 == 1 && $o_4 == 0 )
	{
		$file = $cfg['paths']['db'] . $asn_number . '.6' . $cfg['aggregate']['suffix'];
	}
	// v4 and v6
	else
	{
		$file = $cfg['paths']['db'] . $asn_number . $cfg['aggregate']['suffix'];
	}

	if (!file_exists($file) || !is_readable($file)) {
		status(STATUS_ERROR, "Cannot open IRRDB prefix list file {$file}");
		return FALSE;
	}

	if (!($pfxfile = fopen($file, "r"))) {
		status(STATUS_ERROR, "Cannot open IRRDB prefix list file {$file}");
		return FALSE;
	}

	switch ($format) {
	    case 'cisco':
		pfxlist_generate_cisco($pfxfile, $asn_number, $pfxstr, $pfxstr_v6, $pfxlen, $pfxlen_v6, $o_4, $o_6);
		break;
	    case 'iosxr':
		pfxlist_generate_iosxr($pfxfile, $asn_number, $pfxstr, $pfxstr_v6, $pfxlen, $pfxlen_v6, $o_4, $o_6);
		break;
	    case 'juniper':
		pfxlist_generate_juniper($pfxfile, $asn_number, $pfxstr, $pfxstr_v6, $pfxlen, $pfxlen_v6, $o_4, $o_6);
		break;
	    case 'extreme':
		pfxlist_generate_extreme($pfxfile, $asn_number, $pfxstr, $pfxstr_v6, $pfxlen, $pfxlen_v6, $o_4, $o_6);
		break;
	    case 'force10':
		pfxlist_generate_force10($pfxfile, $asn_number, $pfxstr, $pfxstr_v6, $pfxlen, $pfxlen_v6, $o_4, $o_6);
		break;
	    case 'openbgpd':
		pfxlist_generate_openbgpd($pfxfile, $asn_number, $pfxstr, $pfxstr_v6, $pfxlen, $pfxlen_v6, $o_4, $o_6);
		break;
	    case 'edgeos':
		pfxlist_generate_edgeos($pfxfile, $asn_number, $pfxstr, $pfxstr_v6, $pfxlen, $pfxlen_v6, $o_4, $o_6);
		break;
	    case "huawei":
		pfxlist_generate_huawei($pfxfile, $asn_number, $pfxstr, $pfxstr_v6, $pfxlen, $pfxlen_v6, $o_4, $o_6);
		break;
	    default:
		break;
	}

	fclose($pfxfile);
}

function pfxlist_generate_juniper($pfxfile, $asn, $pfxstr, $pfxstr_v6, $pfxlen, $pfxlen_v6, $o_4, $o_6)
{
	global $cfg;
	$v4_list = $v6_list = array();
	$v4_policy_name = sprintf($pfxstr, $asn);
	$v6_policy_name = sprintf($pfxstr_v6, $asn);

	while (!feof($pfxfile)) {
			if (!($line = rtrim(fgets($pfxfile, 64))))
				continue;

		if (_ip_version($line) == '4')
		{
			$subnet_parts = explode("/", $line);
			if( $subnet_parts[1] > $pfxlen )
				continue;

			if( $subnet_parts[1] == $pfxlen )
			{
				$formatted_line = sprintf("                route-filter %s exact;\n", $line);
			} else {
				$formatted_line = sprintf("                route-filter %s upto /%d;\n", $line, $pfxlen);
			}
			$v4_list[] = $formatted_line;
		}
		elseif (_ip_version($line) == '6')
		{
			$subnet_parts = explode("/", $line);
			if( $subnet_parts[1] > $pfxlen_v6 )
				continue;

			if( $subnet_parts[1] == $pfxlen_v6 )
			{
				$formatted_line = sprintf("                route-filter %s exact;\n", $line);
			} else {
				$formatted_line = sprintf("                route-filter %s prefix-length-range /%d-/%d;\n", $line, $subnet_parts[1], $pfxlen_v6);
			}
			$v6_list[] = $formatted_line;
		}
		else
		{
		  status(STATUS_ERROR, "Prefix not recognized: {$line}");
		  return FALSE;
		}
	}

	pfxlist_print_juniper($v4_policy_name, $v4_list);
	pfxlist_print_juniper($v6_policy_name, $v6_list);
	return 0;
}

function pfxlist_print_juniper($policy_name, $list) {

	if(count($list > 0)) {
		printf("policy-options {\n");
		printf("    replace: policy-statement %s {\n", $policy_name);
		printf("        term prefixes {\n");
		printf("            from {\n");

		foreach($list as $line) {
			print $line;
		}
		printf("            }\n");
		printf("            then next policy;\n");
		printf("        }\n");
		printf("        then reject;\n");
		printf("    }\n");
		printf("}\n");
	}

}

function pfxlist_generate_cisco($pfxfile, $asn, $pfxstr, $pfxstr_v6, $pfxlen, $pfxlen_v6, $o_4, $o_6)
{
	$pfxname = sprintf($pfxstr, $asn);
	$pfxname_v6 = sprintf($pfxstr_v6, $asn);
	global $cfg;

	printf("conf t\n");
  if( $o_4 == 1 && $o_6 == 0 )
  {
    printf("no ip prefix-list %s\n", $pfxname);
  }
  elseif( $o_6 == 1 && $o_4 == 0 )
  {
    printf("no ipv6 prefix-list %s\n", $pfxname_v6);
  }
  else
  {
    printf("no ip prefix-list %s\n", $pfxname);
    printf("no ipv6 prefix-list %s\n", $pfxname_v6);
  }

	while (!feof($pfxfile)) {
		if (!($line = rtrim(fgets($pfxfile, 64))))
			continue;

		$prefix = explode("/", rtrim($line));
	// $prefix[0] = cidr 10.0.0.0
	// $prefix[1] = mask /26

    if (_ip_version($line) == '4')
    {
		if( $prefix[1] > $pfxlen )
			continue;
	
		  printf("ip prefix-list %s permit %s/%d", $pfxname, $prefix[0], $prefix[1]);
  		if ($prefix[1] < $pfxlen)
	  		printf(" le %s", $pfxlen);
    }

    elseif (_ip_version($line) == '6')
    {
		if( $prefix[1] > $pfxlen_v6 )
			continue;
		  printf("ipv6 prefix-list %s permit %s/%d", $pfxname_v6, $prefix[0], $prefix[1]);

		  if ($prefix[1] < $pfxlen_v6)
			  printf(" le %s", $pfxlen_v6);
    }

		printf("\n");
	}

	printf("end\n");
	printf("write mem\n");

	return 0;
}

function pfxlist_generate_iosxr($pfxfile, $asn, $pfxstr, $pfxstr_v6, $pfxlen, $pfxlen_v6, $o_4, $o_6)
{
	$pfxname = sprintf($pfxstr, $asn);
	$pfxname_v6 = sprintf($pfxstr_v6, $asn);
	$pfx1st = 'Y';
	$pfx1st_v6 = 'Y';
	global $cfg;

	printf("conf\n");
  if( $o_4 == 1 && $o_6 == 0 )
  {
    printf("no prefix-set %s\n", $pfxname);
  }
  elseif( $o_6 == 1 && $o_4 == 0 )
  {
    printf("no prefix-set %s\n", $pfxname_v6);
  }
  else
  {
    printf("no prefix-set %s\n", $pfxname);
    printf("no prefix-set %s\n", $pfxname_v6);
  }

  $skipcomma = false;

	while (!feof($pfxfile)) {
		if (!($line = rtrim(fgets($pfxfile, 64))))
			continue;

		$prefix = explode("/", rtrim($line));
		if ($pfx1st_v6 == 'N' && preg_match('/:/', $prefix[0])) {
			if ($skipcomma)
				$skipcomma = false;
			else
				printf(",\n");
		}
		elseif (preg_match('/:/', $prefix[0]) && $pfx1st == 'N')
		{
			printf("\nend-set\n");
			printf("prefix-set %s\n", $pfxname_v6);
			$pfx1st_v6 = 'N';
		}
		elseif ($pfx1st_v6 == 'Y' && preg_match('/:/', $prefix[0]))
		{
			printf("prefix-set %s\n", $pfxname_v6);
			$pfx1st_v6 = 'N';
		}
		elseif ($pfx1st == 'N')
		{
			if ($skipcomma)
				$skipcomma = false;
			else
				printf(",\n");
		}
		else
		{
			$pfx1st = 'N';
    			printf("prefix-set %s\n", $pfxname);
		}


    if (_ip_version($line) == '4')
    {
		if( $prefix[1] > $pfxlen )
		{
			$skipcomma = true;
			continue;
		}

		  printf("  %s/%d", $prefix[0], $prefix[1]);
  		if ($prefix[1] < $pfxlen)
	  		printf(" le %s", $pfxlen);
    }

    elseif (_ip_version($line) == '6')
    {
		if( $prefix[1] > $pfxlen_v6 )
		{
			$skipcomma = true;
			continue;
		}

		  printf("  %s/%d", $prefix[0], $prefix[1]);

		  if ($prefix[1] < $pfxlen_v6)
			  printf(" le %s", $pfxlen_v6);
    }

	}

	if ($pfx1st == 'N' || $pfx1st_v6 == 'N')
	{
		printf("\n");
		printf("end-set\n");
	}
	printf("commit\n");
	printf("exit\n");

	return 0;
}

function pfxlist_generate_extreme($pfxfile, $asn, $pfxstr, $pfxstr_v6, $pfxlen, $pfxlen_v6, $o_4, $o_6)
{
	/* ExtremeWare can not handle : names, replace with - */
	$pfxname = str_replace(":","-", sprintf($pfxstr, $asn));
	$pfxname_v6 = str_replace(":","-", sprintf($pfxstr_v6, $asn));
	global $cfg;

  if( $o_4 == 1 && $o_6 == 0 )
  {
    printf("delete access-profile %s\n", $pfxname);
    printf("create access-profile %s type ipaddress\n", $pfxname);
  }
  elseif( $o_6 == 1 && $o_4 == 0 )
  {
    printf("delete access-profile %s\n", $pfxname_v6);
    printf("create access-profile %s type ipaddress\n", $pfxname_v6);
  }
  else
  {
    printf("delete access-profile %s\n", $pfxname);
    printf("create access-profile %s type ipaddress\n", $pfxname);
    printf("delete access-profile %s\n", $pfxname_v6);
    printf("create access-profile %s type ipaddress\n", $pfxname_v6);
  }

	$counter = 10;

	while (!feof($pfxfile)) {
		if (!($line = rtrim(fgets($pfxfile, 64))))
			continue;

		$prefix = explode("/", rtrim($line));


    if (_ip_version($line) == '4')
    {
		if( $prefix[1] > $pfxlen )
			continue;

		  printf("configure access-profile %s add %d permit ipaddress %s/%d", $pfxname, $counter += 10, $prefix[0], $prefix[1]);
		  if ($prefix[1] < $pfxlen)
      {
			  printf("\n", $pfxlen);
      }
      else
      {
	      printf(" exact\n");
      }
    }
    elseif (_ip_version($line) == '6')
    {
		if( $prefix[1] > $pfxlen_v6 )
			continue;

		  printf("configure access-profile %s add %d permit ipaddress %s/%d", $pfxname_v6, $counter += 10, $prefix[0], $prefix[1]);
		  if ($prefix[1] < $pfxlen_v6)
      {
			  printf("\n", $pfxlen_v6);
      }
      else
      {
	      printf(" exact\n");
      }
    }
	}

	printf("save\n");

	return 0;
}

function pfxlist_generate_force10($pfxfile, $asn, $pfxstr, $pfxstr_v6, $pfxlen, $pfxlen_v6, $o_4, $o_6)
{
	$pfxname = sprintf($pfxstr, $asn);
	$pfxname_v6 = sprintf($pfxstr_v6, $asn);
	$v4_list = $v6_list = array();
	global $cfg;

	printf("conf t\n");

    while (!feof($pfxfile)) {
        if (!($line = rtrim(fgets($pfxfile, 64))))
            continue;

		$prefix = explode("/", rtrim($line));

		if (_ip_version($line) == '4')
		{
			if( $prefix[1] > $pfxlen )
				continue;

			$formatted_line = sprintf("  permit %s/%d", $prefix[0], $prefix[1]);
			if ($prefix[1] < $pfxlen)
				$formatted_line .= sprintf(" le %s", $pfxlen);

			$v4_list[] = $formatted_line;
		}
		elseif (_ip_version($line) == '6')
		{
			if( $prefix[1] > $pfxlen_v6 )
				continue;

			$formatted_line = sprintf("  permit %s/%d", $prefix[0], $prefix[1]);
			if ($prefix[1] < $pfxlen_v6)
				$formatted_line .= sprintf(" le %s", $pfxlen_v6);

			$v6_list[] = $formatted_line;
		}
		else
		{
		  status(STATUS_ERROR, "Prefix not recognized: {$line}");
		  return FALSE;
		}
	} //end while

	pfxlist_print_force10($pfxname, $v4_list, 4);
	pfxlist_print_force10($pfxname_v6, $v6_list, 6);

	printf("end\n");
	printf("write mem\n");

	return 0;
}

function pfxlist_print_force10($policy_name, $list, $AF)
{
	if( count($list) > 0 )
	{
	    if( $AF == 4 )
	    {
		printf("no ip prefix-list %s\n", $policy_name);
		printf("ip prefix-list %s\n", $policy_name);
	    } else {
		printf("no ipv6 prefix-list %s\n", $policy_name);
		printf("ipv6 prefix-list %s\n", $policy_name);
	    }
	}

        foreach( $list as $line )
	{
            print $line;
	    printf("\n");
        }
}

function pfxlist_generate_openbgpd($pfxfile, $asn, $pfxstr, $pfxstr_v6, $pfxlen, $pfxlen_v6, $o_4, $o_6)
{
	$pfxname = sprintf($pfxstr, $asn);
	$pfxname_v6 = sprintf($pfxstr_v6, $asn);
	$v4_list = $v6_list = array();
	global $cfg;

	// printf("conf t\n");

	while (!feof($pfxfile)) {
          if (!($line = rtrim(fgets($pfxfile, 64))))
            continue;

		$prefix = explode("/", rtrim($line));

		if (_ip_version($line) == '4')
		{
			if( $prefix[1] > $pfxlen )
				continue;

			$formatted_line = sprintf(" allow from any peer-as %d prefix %s/%d", $asn, $prefix[0], $prefix[1]);
			if ($prefix[1] < $pfxlen)
				$formatted_line .= sprintf(" <= %s", $pfxlen);

			$v4_list[] = $formatted_line;
			printf($formatted_line . "\n");
		}
		elseif (_ip_version($line) == '6')
		{
			if( $prefix[1] > $pfxlen_v6 )
				continue;

			$formatted_line = sprintf(" allow from any peer-as %d prefix %s/%d", $asn, $prefix[0], $prefix[1]);
			if ($prefix[1] < $pfxlen_v6)
				$formatted_line .= sprintf(" <= %s", $pfxlen_v6);

			$v6_list[] = $formatted_line;
			printf($formatted_line . "\n");
		}
		else
		{
		  status(STATUS_ERROR, "Prefix not recognized: {$line}");
		  return FALSE;
		}
	} //end while

	// pfxlist_print_openbgpd($pfxname, $v4_list, 4);
	// pfxlist_print_openbgpd($pfxname_v6, $v6_list, 6);

	return 0;
}

function pfxlist_generate_edgeos($pfxfile, $asn, $pfxstr, $pfxstr_v6, $pfxlen, $pfxlen_v6, $o_4, $o_6)
{
	$pfxname = sprintf($pfxstr, $asn);
	$pfxname_v6 = sprintf($pfxstr_v6, $asn);
	global $cfg;
	$rule = 0;
	$rule_v6 = 0;

	printf("configure\n");

	if( $o_4 == 1 && $o_6 == 0 )
	{
		printf("delete policy prefix-list %s\n", $pfxname);
	}
	elseif( $o_6 == 1 && $o_4 == 0 )
	{
		printf("delete policy prefix-list6 %s\n", $pfxname_v6);
	}
	else
	{
		printf("delete policy prefix-list %s\n", $pfxname);
		printf("delete policy prefix-list6 %s\n", $pfxname_v6);
	}

	while (!feof($pfxfile)) {
          if (!($line = rtrim(fgets($pfxfile, 64))))
            continue;

		$prefix = explode("/", rtrim($line));

		if (_ip_version($line) == '4')
		{
			if( $prefix[1] > $pfxlen )
				continue;

			$rule = $rule + 1;

			printf("set policy prefix-list %s rule %d prefix %s/%d\n", $pfxname, $rule, $prefix[0], $prefix[1]);

			if ($prefix[1] < $pfxlen)
				printf("set policy prefix-list %s rule %d le %s\n", $pfxname, $rule, $pfxlen);
		}

		elseif (_ip_version($line) == '6')
		{

			if( $prefix[1] > $pfxlen_v6 )
				continue;

			$rule_v6 = $rule_v6 + 1;

			printf("set policy prefix-list6 %s rule %d prefix %s/%d\n", $pfxname_v6, $rule_v6, $prefix[0], $prefix[1]);

			if ($prefix[1] < $pfxlen_v6)
				printf("set policy prefix-list6 %s rule %d le %s\n", $pfxname_v6, $rule_v6, $pfxlen_v6);

		}
		else
		{
		  status(STATUS_ERROR, "Prefix not recognized: {$line}");
		  return FALSE;
		}
	} //end while

	printf("commit\n");
	printf("save\n");

	return 0;
}

function pfxlist_generate_huawei($pfxfile, $asn, $pfxstr, $pfxstr_v6, $pfxlen, $pfxlen_v6, $o_4, $o_6) {
	$pfxname = sprintf($pfxstr, $asn);
	$pfxname_v6 = sprintf($pfxstr_v6, $asn);
	global $cfg;

	printf("system-view\n");

	if( $o_4 == 1 && $o_6 == 0 )
	{
		printf("undo ip ip-prefix %s\n", $pfxname);
	}
	elseif( $o_6 == 1 && $o_4 == 0 )
	{
		printf("undo ip ipv6-prefix %s\n", $pfxname_v6);
	}
	else
	{
		printf("undo ip ip-prefix %s\n", $pfxname);
		printf("undo ip ipv6-prefix %s\n", $pfxname_v6);
	}

	while (!feof($pfxfile)) {
		if (!($line = rtrim(fgets($pfxfile, 64))))
			continue;

		$prefix = explode("/", rtrim($line));

		if (_ip_version($line) == '4')
		{
			if( $prefix[1] > $pfxlen )
				continue;
	
			printf("ip ip-prefix %s permit %s %d", $pfxname, $prefix[0], $prefix[1]);
			if ($prefix[0] == "0.0.0.0") {
				printf(" match-network");
			}
			if ($prefix[1] < $pfxlen)
				printf(" le %s", $pfxlen);
			}

		elseif (_ip_version($line) == '6')
		{
			if( $prefix[1] > $pfxlen_v6 )
				continue;
        
			printf("ip ipv6-prefix %s permit %s %d", $pfxname_v6, $prefix[0], $prefix[1]);
			if ($prefix[0] == "::")
				printf(" match-network");

			if ($prefix[1] < $pfxlen_v6)
				printf(" le %s", $pfxlen_v6);
			}

		printf("\n");
	}

	printf("return\n");
	printf("save\n");
	printf("y\n");

	return 0;
}

?>
