local M = {}

M.windows = {} -- track the open windows

function M.track_window(win)

  if #M.windows == 0 then
    print("[vimasm] server setup code!")
  end

  table.insert(M.windows, win)
  -- autocmd to remove window from tracking when closed
  vim.api.nvim_create_autocmd("WinClosed", {
    once = true, 
    callback = function(args)
      local closed_win = tonumber(args.match)
      for i, id in ipairs(M.windows) do
          if id == closed_win then
            table.remove(M.windows, i)
            break
          end
      end
      if #M.windows == 0 then
        print("[vimasm] goodbye")
      end
    end
  })
end


function M.open_horizontal()
    local buf = vim.api.nvim_create_buf(false, true)
    vim.cmd("split")

    local win = vim.api.nvim_get_current_win()
    M.track_window(win)

    vim.api.nvim_win_set_buf(0, buf)
    vim.api.nvim_win_set_height(0, 15)
end


function M.open_vertical()
    local buf = vim.api.nvim_create_buf(false, true)
    vim.cmd("vsplit")

    local win = vim.api.nvim_get_current_win()
    M.track_window(win)

    vim.api.nvim_win_set_buf(0, buf)
    vim.api.nvim_win_set_width(0, 50)
end


function M.setup()
    vim.api.nvim_create_user_command("VimasmHSplit", M.open_horizontal, 
      { desc = "Open horizontal split window" })

    vim.api.nvim_create_user_command("VimasmVSplit", M.open_vertical, 
      { desc = "Open vertical split window" })

    vim.api.nvim_create_user_command("VimasmHello", function()
        print("[vimasm] Hello from plugin!")
    end, { desc = "Echo Hello test from plugin" })
end


return M
