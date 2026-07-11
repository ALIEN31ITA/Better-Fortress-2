//=========== (C) Copyright 2005 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//=============================================================================

// No spaces in event names, max length 32
// All strings are case sensitive
//
// valid data key types are:
//   string : a zero terminated string
//   bool   : unsigned int, 1 bit
//   byte   : unsigned int, 8 bit
//   short  : signed int, 16 bit
//   long   : signed int, 32 bit
//   float  : float, 32 bit
//   local  : any data, but not networked to clients
//
// following key names are reserved:
//   local      : if set to 1, event is not networked to clients
//   unreliable : networked, but unreliable
//   suppress   : never fire this event
//   time	: firing server time
//   eventid	: holds the event ID

"ModEvents"
{

	// a game event, name may be 32 charaters long
	"generic_killfeed_event"		
	{
		"victim"				"short"   	// user ID who died, used to display involvement
		"killer"				"short"	 	// user ID who killed, used to display involvement
		"victim_team"			"short"   	// team of victim, used to display base team colors
		"killer_team"			"short"	 	// team of killer, used to display base team colors
		"victim_name"			"string" 	// name of the victim
		"killer_name"			"string" 	// name of the killer
		"msg"					"string"	// text to display after killer
		"msg2"					"string"	// text to display after victim
		"msg3"					"string"	// text to display after killer before msg
		"msgcolor"				"string" 	// color of the text to display after killer in "R G B A"
		"msgcolor2"				"string" 	// color of the text to display after victim in "R G B A"
		"msgcolor3"				"string" 	// color of the text to display after killer before msg in "R G B A"
		"msgcolor_victim"		"string" 	// color of the victim in "R G B A"
		"msgcolor_killer"		"string" 	// color of the killer in "R G B A"
		"killicon"				"string"	// name of the killicon
		"killicon_background"	"string"	// name of the killicon's background, used for australium / crit
	}
	
	// For detecting taunts
	"cf_player_taunt"		
	{
		"taunter"				"short"   	// user ID who started taunting
		"partner"				"short"	 	// user ID who joined the taunt
		"taunter_tauntID"		"long"   	// taunt schema ID of the taunter
		"partner_tauntID"		"long"		// taunt schema ID of the partner
		"attack_name"			"long"		// Used by TauntKills
	}
}

