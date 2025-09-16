import os

# Simple approach - read .env and output build flags
if os.path.exists('.env'):
    with open('.env', 'r') as f:
        lines = f.readlines()
    
    flags = []
    for line in lines:
        line = line.strip()
        if line and not line.startswith('#') and not line.startswith(';') and '=' in line:
            key, value = line.split('=', 1)
            key = key.strip()
            value = value.strip()
            
            # Remove surrounding quotes if present
            if value.startswith('"') and value.endswith('"'):
                value = value[1:-1]
            if value.startswith("'") and value.endswith("'"):
                value = value[1:-1]
            
            # Output the flag with proper escaping for Windows
            flags.append(f'-D{key}=\\"{value}\\"')
    
    print(' '.join(flags))
else:
    print('')  # Empty output if no .env file