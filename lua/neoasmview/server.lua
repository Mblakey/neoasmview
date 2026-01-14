local M = {}

local uv = vim.loop

M.root_dir = ""
M.socket_path = nil
M.startup_done  = false
M.startup_error = false
M.augroup = nil

M.file_to_buf = {}
M.buf_to_file = {}

M.handle = nil
M.stdout = nil
M.stderr = nil
M.client = nil


local bit = require("bit")  -- LuaJIT / Neovim

local function read_uint32_le(s)
  local b1,b2,b3,b4 = s:byte(1,4)
  return b1 + bit.lshift(b2,8) + bit.lshift(b3,16) + bit.lshift(b4,24)
end


function M.set_root(path)
  if path == nil or path == "" then
    vim.notify("[vimasm] Invalid path", vim.log.levels.WARN)
    return
  end
  M.root = path
  vim.notify("[vimasm] project root set to: " .. path, vim.log.levels.INFO)
end


function M.start()
  M.stdout = uv.new_pipe(false)
  M.stderr = uv.new_pipe(false)
  M.client = uv.new_pipe(false)

  M.handle = uv.spawn("/home/mikey/Workspace/neoasmview/build/asm-server", 
                      { 
                        args = {M.root},
                        stdio = {nil, M.stdout, M.stderr},
                      }, 
                      function (code, signal)
                        M.stop()
                        print("[vimasm] asm-server exited", code, signal)
                      end
                      )

  M.stderr:read_start(vim.schedule_wrap(function(_, data)
    if data then
      vim.schedule(function()
        print("[asm-socket]", data)
      end)
    end
  end))

  M.stdout:read_start(vim.schedule_wrap(function(_, data)
    if not data or M.startup_done then
      return
    end

    local path = data:gsub("\r?\n$", "")

    if path == "VIMASM_NULL" then
      M.startup_error = true
    else
      M.socket_path = path
    end

    M.startup_done = true
  end))

  -- wait synchronously 
  vim.wait(5000, function()
    return M.startup_done 
  end, 50)

  if M.startup_error then
    M.stop()
    return;
  end

  local connected = false

  M.client:connect(M.socket_path, function(err)
    if err then
      print("[vimasm] Failed to connect:", err)
      M.stop()
      return
    end

    connected = true
    -- set up our socket handler, not guaranteed to read the 
    -- whole message in one go, prefix with expected length,
    -- store in buffer
    
    local buffer = ""
    local msg_size = nil
    M.client:read_start(vim.schedule_wrap(function(_, data)
      if not data then 
        return
      end
      
      buffer = buffer .. data 

      if not msg_size and #buffer >= 4 then 
        msg_size =  read_uint32_le(buffer:sub(1,4))
        buffer = buffer:sub(5) -- should be pure json after this
      end

      if msg_size and #buffer >= msg_size then
        local ok, json_obj = pcall(vim.json.decode, buffer)
        buffer = "" -- reset the buffer
        if not ok then
          print("[vimasm] invalid JSON request")
          return
        end

        local filepath = json_obj.filepath
        local asm = json_obj.asm

        M.send_to_buffer(filepath, asm)
      end
    end))
  end)

  local ok = vim.wait(5000, function()
    return connected
  end)
  
  M.augroup = vim.api.nvim_create_augroup("VIMASM", {clear = true})
  return ok
end


function M.stop()
  if M.augroup then
    vim.api.nvim_clear_autocmds({ group = M.augroup })
    M.augroup = nil
  end

  if M.stdout then M.stdout:close() M.stdout=nil end
  if M.stderr then M.stderr:close() M.stderr=nil end
  if M.client then M.client:close() M.client=nil end
  
  if M.handle then 
    M.handle:kill("sigint")
    M.handle:close() 
    M.handle=nil 
  end

  M.socket_path = nil
  M.startup_done  = false
  M.startup_error = false
end


function M.register_buffer(filename, bufnr)
  local path = vim.fn.fnamemodify(filename, ":p")
  M.file_to_buf[path] = bufnr
  M.buf_to_file[bufnr] = path

  vim.api.nvim_create_autocmd({"BufWipeout", "BufDelete"}, {
    buffer = bufnr, 
    group = M.augroup,
    callback = function()
      M.file_to_buf[path] = nil
      M.buf_to_file[bufnr] = nil

      -- If no buffers left, stop server
      if vim.tbl_isempty(M.file_to_buf) then
        M.stop()
      end
    end
  })
end


function M.open_assembly_vertical()
    if M.startup_done == false then
      local ok = M.start()
      if not ok then return end
    end
    
    local filename = M.get_current_buffer_path()
    local ft = vim.api.nvim_buf_get_option(0, "filetype")
    local buf = M.file_to_buf[filename] 

    if buf then 
      local cur_win = vim.api.nvim_get_current_win()
      vim.cmd("vsplit")
      vim.api.nvim_set_current_win(cur_win)

      vim.api.nvim_win_set_buf(0, buf)
      vim.api.nvim_win_set_width(0, 50)
      return
    end 

    buf = vim.api.nvim_create_buf(false, true)

    vim.api.nvim_buf_set_option(buf, "buftype", "nofile")   
    vim.api.nvim_buf_set_option(buf, "bufhidden", "wipe")  
    vim.api.nvim_buf_set_option(buf, "swapfile", false)   
    vim.api.nvim_buf_set_option(buf, "modifiable", false) 
    vim.api.nvim_buf_set_option(buf, "filetype", "asm")  

    M.register_buffer(filename, buf)

    local cur_win = vim.api.nvim_get_current_win()
    local cur_buf = vim.api.nvim_get_current_buf()
    vim.cmd("vsplit")
    vim.api.nvim_set_current_win(cur_win)

    vim.api.nvim_win_set_buf(0, buf)
    vim.api.nvim_win_set_width(0, 50)

    vim.api.nvim_create_autocmd("BufWritePost", {
      buffer = cur_buf, 
      group = M.augroup,
      callback = function()
        M.send_assembly_request(filename)
      end
    })
    
    M.send_assembly_request(filename)
end


function M.open_functions_vertical()
    if M.startup_done == false then
      local ok = M.start()
      if not ok then return end
    end
    
    local filename = M.get_current_buffer_path()
    local ft = vim.api.nvim_buf_get_option(0, "filetype")
    local buf = M.file_to_buf[filename] 

    if buf then 
      local cur_win = vim.api.nvim_get_current_win()
      vim.cmd("vsplit")
      vim.api.nvim_set_current_win(cur_win)

      vim.api.nvim_win_set_buf(0, buf)
      vim.api.nvim_win_set_width(0, 50)
      return
    end 

    buf = vim.api.nvim_create_buf(false, true)

    vim.api.nvim_buf_set_option(buf, "buftype", "nofile")   
    vim.api.nvim_buf_set_option(buf, "bufhidden", "wipe")  
    vim.api.nvim_buf_set_option(buf, "swapfile", false)   
    vim.api.nvim_buf_set_option(buf, "modifiable", false) 
    vim.api.nvim_buf_set_option(buf, "filetype", "asm")  

    M.register_buffer(filename, buf)

    local cur_win = vim.api.nvim_get_current_win()
    local cur_buf = vim.api.nvim_get_current_buf()
    vim.cmd("vsplit")
    vim.api.nvim_set_current_win(cur_win)

    vim.api.nvim_win_set_buf(0, buf)
    vim.api.nvim_win_set_width(0, 50)

    M.send_function_request(filename)
end


function M.send_assembly_request(filename)
  if not M.startup_done then
    print("[vimasm] server socket not available")
    return
  end
  
  local request = {
    filepath = filename,
    command = "assembly",
  }
  
  local json = vim.json.encode(request) .. "\n"
  
  uv.write(M.client, json, function(err)
    if err then
      print("[vimasm] failed to write to server socket:", err)
    end
  end)
end


function M.send_function_request(filename)
  if not M.startup_done then
    print("[vimasm] server socket not available")
    return
  end
  
  local request = {
    filepath = filename,
    command = "functions",
  }
  
  local json = vim.json.encode(request) .. "\n"
  
  uv.write(M.client, json, function(err)
    if err then
      print("[vimasm] failed to write to server socket:", err)
    end
  end)
end


function M.get_buf_filename(bufid)
  bufid = bufid or 0  -- default to current buffer
  local name = vim.api.nvim_buf_get_name(bufid)
  if name == "" then
    return nil  
  end

  return vim.fn.fnamemodify(name, ":p")  -- absolute path
end


function M.get_current_buffer_path()
  local bufid = 0
  local name = vim.api.nvim_buf_get_name(bufid)
  if name == "" then return nil end  -- unsaved buffer
  return vim.fn.fnamemodify(name, ":p")
end


-- multiplex on file path
function M.send_to_buffer(filename, data)
  local bufid = M.file_to_buf[filename]
  if bufid == nil then
    print("[vimasm] no buffer associated with file" .. filename)
    return
  end
  
  local lines = vim.split(data, "\n", { plain = true, trimempty = false })
  
  vim.api.nvim_buf_set_option(bufid, "modifiable", true)
  vim.api.nvim_buf_set_lines(bufid, 0, -1, false, lines)
  vim.api.nvim_buf_set_option(bufid, "modifiable", false)
end

function M.setup() 
  M.root_dir = vim.fn.getcwd()

  vim.api.nvim_create_autocmd("VimLeavePre", {
    group = M.augroup,
    callback = function()
      M.stop()
    end
  })

  vim.api.nvim_create_user_command(
    "VimasmVSplitAsm", M.open_assembly_vertical, {}
  )

  vim.api.nvim_create_user_command(
    "VimasmVSplitFunctions", M.open_functions_vertical, {}
  )
  
  vim.api.nvim_create_user_command(
    "VimasmASM",                 
    function()                  
      local filename = M.get_buf_filename()  
      if not filename then
        vim.notify("[vimasm] No file associated with current buffer", vim.log.levels.WARN)
        return
      end
      M.send_assembly_request(filename)
    end, {}
  )
end

return M
