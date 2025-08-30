// tests extended functionality for the CScriptKeyValues class
function recursiveDumpKeys(keys, prefix = "")
{
	for(local key = keys.GetFirstSubKey(); key != null; key = key.GetNextKey())
	{
		local recursive = false, val;
		switch(key.GetDataType())
		{
			case Constants.EKeyValueType.TYPE_STRING:
				val = key.GetString()
			break
			
			case Constants.EKeyValueType.TYPE_INT:
				val = key.GetInt()
			break
			
			case Constants.EKeyValueType.TYPE_FLOAT:
				val = key.GetFloat()
			break
			
			// it's better to use "default:" here,
			// but we're just showing off the new enums we have.
			case Constants.EKeyValueType.TYPE_NONE:
			case Constants.EKeyValueType.TYPE_COLOR:
			case Constants.EKeyValueType.TYPE_PTR:
			case Constants.EKeyValueType.TYPE_WSTRING:
			case Constants.EKeyValueType.TYPE_NUMTYPE:
				if(!keys.IsEmpty())
				{
					recursiveDumpKeys(key, prefix + key.GetName() + ".")
					recursive = true
				}
			break
		}
		
		if(val != null)
			printf("%s%s: (%s) %s\n", prefix, key.GetName(), typeof val, val.tostring())
		else if(!recursive)
			printf("%s%s: (NULL) NULL\n", prefix, key.GetName())
	}
}


local keys = FileToKeyValues("betterfortress/keyvalues_example.txt")
printf("HEADER: %s\n", keys.GetName())
recursiveDumpKeys(keys)