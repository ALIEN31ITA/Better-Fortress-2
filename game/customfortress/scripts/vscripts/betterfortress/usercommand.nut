// console commands:
// scripted_user_func size bigger
// scripted_user_func size big
// scripted_user_func size normal
// scripted_user_func size small
// scripted_user_func size smaller
//
// read code to understand it more

function UserConsoleCommand(player, arg)
{
	switch(arg)
	{
		
		case "size bigger":
			player.SetModelScale(3, 1)
		break
		
		case "size big":
			player.SetModelScale(2, 1)
		break
		
		case "size normal":
			player.SetModelScale(1, 1)
		break
		
		case "size small":
			player.SetModelScale(0.5, 1)
		break
		
		case "size smaller":
			player.SetModelScale(0.25, 1)
		break
	}
}
