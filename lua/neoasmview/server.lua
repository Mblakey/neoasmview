local M = {}

local uv = vim.loop

M.root_dir = ""
M.socket_path = nil
M.startup_done  = false
M.startup_error = false

M.file_to_buf = {}
M.buf_to_file = {}

M.handle = nil
M.stdout = nil
M.stderr = nil
M.client = nil

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
                        if M.stdout then M.stdout:close() M.stdout=nil end
                        if M.stderr then M.stderr:close() M.stderr=nil end
                        if M.client then M.client:close() M.client=nil end
                        if M.handle then M.handle:close() M.handle=nil end

                        M.startup_done  = false
                        M.startup_error = false
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
    if not data or startup_done then
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

  if M.startup_error == true then
    return false
  end

  local connected = false

  M.client:connect(M.socket_path, function(err)
    if err then
      print("[vimasm] Failed to connect:", err)
      return
    end

    connected = true

    -- set up our socket handler
    M.client:read_start(vim.schedule_wrap(function(_, data)
      if not data then 
        return
      end
      
      local payload = data:sub(7)  -- everything after "VIMASM"
      local pos = string.find(payload, "\0")

      if pos then
        filepath = string.sub(payload, 1, pos - 1)
        rest = string.sub(payload, pos + 1)
      end
      
      M.send_to_buffer(filepath, rest)
    end))
  end)

  local ok = vim.wait(5000, function()
    return connected
  end)

  return ok
end


function M.stop()
  if not M.handle then
    return
  end
  M.handle:kill("sigint")
  M.handle      = nil
  M.socket_path = nil
  M.client      = nil

  M.startup_done  = false
  M.startup_error = false
end


function M.register_buffer(filename, bufnr)
  local path = vim.fn.fnamemodify(filename, ":p")
  M.file_to_buf[path] = bufnr
  M.buf_to_file[bufnr] = path

  vim.api.nvim_create_autocmd({"BufWipeout", "BufDelete"}, {
    buffer = bufnr,
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


function M.open_vertical()
    if M.startup_done == false then
      local ok = M.start()
      if not ok then return end
    end
    
    local filename = M.get_current_buffer_path()
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
      callback = function()
        M.send_request(filename)
      end
    })
    
    M.send_request(filename)
end


function M.send_request(filename)
  if not M.startup_done then
    print("[vimasm] server socket not available")
    return
  end

  local request = filename .. "\n"
  
  uv.write(M.client, request, function(err)
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
    callback = function()
      M.stop()
    end
  })

  vim.api.nvim_create_user_command(
    "VimasmVSplit", M.open_vertical, {}
  )
  
  vim.api.nvim_create_user_command(
    "VimasmASM",                 
    function()                  
      local filename = M.get_buf_filename()  
      if not filename then
        vim.notify("[vimasm] No file associated with current buffer", vim.log.levels.WARN)
        return
      end
      M.send_request(filename)
    end, {}
  )
end

return M
