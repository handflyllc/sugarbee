local test_case = "normal"
-- Possible values:
-- normal, absent, shutdown_error, init_error, update_error


function init()
    print("Shutdown test:", test_case)

    if test_case == "init_error" then
        error("Intentional initialization failure")
    end
end

function update(dt)
    if test_case == "update_error" then
        error("Intentional update failure")
    end
end

function draw()
    sugarbee.cls(8,12,32)
    sugarbee.print("CLOSE WINDOW TO TEST SHUTDOWN", 40, 40)
end

if test_case ~= "absent" then
    function shutdown()
        print("Shutdown callback entered")

        if test_case == "shutdown_error" then
            error("Intentional shutdown failure")
        end

        print("Shutdown callback completed")
    end
end
