<?php
	/*
	Author: Spencer Brydges
	Basic PHP script for handling client logins and the update of client statistics
	To-do: 
	() Work on update/insert statements for scores
	*/

	include 'config.php'; //Contains database details, update file contents when deploying files to LU

	/*
	u = username, p = password (logins)
	s = update score (supply u as well)
	w = increment wins
	l = increment losses
	*/

	$getopts = getopt("u:p:s:");
	$conn = mysql_connect($db_host . ': ' . $db_port, $db_username, $db_password) or die('0x1A'); //Failed to connect to server, relay back to game server
        mysql_select_db($db_database) or die('0x1B'); //Database does not seem to exist, LOG this issue on server

	foreach($getopts as $k => $v) //Make sure every piece of data is sanitized as packets can be modified to initiate an SQLI attack ;)
	{
		$getopts[$k] = mysql_real_escape_string($getopts[$k]);
	}
	if(isset($getopts['u']) && isset($getopts['p'])) //Process client login
	{
		$username = $getopts['u'];
		$password = $getopts['p'];
		$query = mysql_query("SELECT username, password FROM game_users WHERE username = '$username' AND password = '$password'");
		if(mysql_num_rows($query) > 0) 
			print 'P';
		else
			print 'F'; //Invalid login, notify server -> client
	}
	elseif(isset($getopts['s']) && isset($getopts['u'])) //Process win/loss record
	{
		$username = $getopts['u'];
		if(isset($getopts['w']))
		{
			$curr = mysql_query("SELECT wins FROM scores WHERE username = '$username'");
			$curr++;
			mysql_query("UPDATE scores SET wins = '$curr' WHERE username = '$username'");
		}
		else
		{
			$curr = mysql_query("SELECT losses FROM scores WHERE username = '$username'");
			$curr++;
			mysql_query("UPDATE scores SET losses = '$curr' WHERE username = '$username'");
		}
	}
	else //This should never hit, do nothing
	{
	}
	mysql_close($conn); //Cleanup connection
?>
