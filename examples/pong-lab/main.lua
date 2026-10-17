local screen_height = 720
local screen_width = 1280

local paddle_x = 60
local paddle_y = 300
local paddle_width = 20
local paddle_height = 120
local paddle_speed = 300

local ball_size = 16
local ball_x = 632
local ball_y = 352
local ball_velocity_y = 240
local ball_velocity_x = 200

local misses = 0

function init()
    print("Pong Lab: paddle test loaded")

    if math.random(0, 1) == 0 then
        ball_velocity_x = -ball_velocity_x
    end

    if math.random(0, 1) == 0 then
        ball_velocity_y = -ball_velocity_y
    end
end

function update(dt)
    if sugarbee.btn("up") then
        paddle_y = paddle_y - paddle_speed * dt
    end

    if sugarbee.btn("down") then
        paddle_y = paddle_y + paddle_speed * dt
    end

    paddle_y = math.max(
        0,
        math.min(screen_height - paddle_height, paddle_y)
    )

    -- ball movement
    ball_x = ball_x + ball_velocity_x * dt
    ball_y = ball_y + ball_velocity_y * dt

    -- screen boundary check - vertical
    if ball_y + ball_size >= screen_height then
        ball_y = screen_height - ball_size
        ball_velocity_y = -ball_velocity_y
    elseif ball_y <= 0 then
        ball_y = 0
        ball_velocity_y = -ball_velocity_y
    end

    -- screen boundary check - horizontal
    if ball_x + ball_size >= screen_width then
        ball_x = screen_width - ball_size
        ball_velocity_x = -ball_velocity_x
    elseif ball_x <= 0 then
        -- record a point
        misses = misses + 1
        -- return ball to center of the screen
        ball_x = (screen_width - ball_size) / 2
        ball_y = (screen_height - ball_size) / 2
        ball_velocity_x = math.abs(ball_velocity_x)
        -- reset random trajectory
        if math.random(0, 1) == 0 then
            ball_velocity_y = -ball_velocity_y
        end
    end

    -- detect bounce off left paddle
    if ball_velocity_x < 0
        and ball_x < paddle_x + paddle_width
        and ball_x + ball_size > paddle_x
        and ball_y < paddle_y + paddle_height
        and ball_y + ball_size > paddle_y then

        ball_x = paddle_x + paddle_width
        ball_velocity_x = -ball_velocity_x
    end

end

function draw()
    -- clear screen, set background color
    sugarbee.cls(34, 36, 28)

    -- draw a cosmetic play field in layers,
    -- three rectangles overlapping each other
    sugarbee.rect(
        20, 20,
        1240, 680,
        51, 51, 51, true
    )

    sugarbee.rect(
        40, 40,
        1200, 640,
        34, 36, 28, true
    )

    sugarbee.rect(
        630, 60,
        20, 600,
        51, 51, 51, true
    )

    -- draw left paddle
    sugarbee.rect(
        paddle_x, math.floor(paddle_y),
        paddle_width, paddle_height,
        80, 220, 160, true
    )

    -- draw right paddle
    -- TBD

    -- draw ball
    sugarbee.rect(
        math.floor(ball_x), math.floor(ball_y),
        ball_size, ball_size,
        240, 230, 190, true
    )

    sugarbee.print("PONG LAB - PADDLE TEST", 160, 30)
    sugarbee.print("MOVE: UP/DOWN OR W/S", 160, 60)
    sugarbee.print("MISSES: " .. misses, 160, 90)
end