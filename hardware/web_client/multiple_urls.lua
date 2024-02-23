-- Resource: https://github.com/timotta/wrk-scripts/blob/master/multiplepaths.lua

-- Initialize the pseudo random number generator
-- Resource: http://lua-users.org/wiki/MathLibraryTutorial
math.randomseed(0)
math.random(); math.random(); math.random()

-- Shuffle array
-- Returns a randomly shuffled array
function shuffle(paths)
  local j, k
  local n = #paths

  for i = 1, n do
    j, k = math.random(n), math.random(n)
    paths[j], paths[k] = paths[k], paths[j]
  end

  return paths
end

-- Load URL paths from the file
function load_url_paths_from_file(file)
  lines = {}

  -- Check if the file exists
  -- Resource: http://stackoverflow.com/a/4991602/325852
  local f=io.open(file,"r")
  if f~=nil then 
    io.close(f)
  else
    -- Return the empty array
    return lines
  end

  -- If the file exists loop through all its lines 
  -- and add them into the lines array
  for line in io.lines(file) do
    if not (line == '') then
      line = "/web/medical-datasets/diseases/" .. line
      lines[#lines + 1] = line
    end
  end

  return shuffle(lines)
end

-- Load URL paths from file
paths = load_url_paths_from_file("../../dataset/client/webpages.txt")

-- Check if at least one path was found in the file
if #paths <= 0 then
  print("multiplepaths: No paths found. You have to create a file paths.txt with one path per line")
  os.exit()
end

print("multiplepaths: Found " .. #paths .. " paths")

-- Initialize the paths array iterator
counter = 0

request = function()
  -- Get the next paths array element
  url_path = paths[counter]

  -- 
  counter = counter + 1

  -- If the counter is longer than the paths array length then reset it
  if counter > #paths then
    counter = 0
  end

  -- Return the request object with the current URL path
  return wrk.format(nil, url_path)
end