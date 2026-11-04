local screen_height = 720
local screen_width = 1280

local paddle_x = 60
local paddle_y = 300
local paddle_width = 20
local paddle_height = 120
local paddle_speed = 310
local right_paddle_x = screen_width - paddle_x - paddle_width
local right_paddle_y = paddle_y

local ball_size = 16
local ball_x = 632
local ball_y = 352
local ball_velocity_y = 240
local ball_velocity_x = 200

local left_score = 0
local right_score = 0
local winning_score = 5
local winner = nil

-- Separate voices let simultaneous wall/paddle contacts overlap.
local sound_time = {0, 0}
local function hit_sound(voice, hz)
    if sugarbee.synth.note_on(voice, hz) then sound_time[voice] = .025 end
end
local function update_sounds(dt)
    for voice = 1, 2 do
        if sound_time[voice] > 0 then
            sound_time[voice] = math.max(0, sound_time[voice] - dt)
            if sound_time[voice] == 0 then sugarbee.synth.note_off(voice) end
        end
    end
end

function init()
    print("Pong Lab: paddle test loaded")
    for voice = 1, 2 do
        sugarbee.synth.configure(voice, {
            waveform = "triangle", attack = .003, decay = 0,
            sustain = .7, release = .06, volume = .6, pan = 0,
            filter = {bypass = true}
        })
    end

    if math.random(0, 1) == 0 then
        ball_velocity_x = -ball_velocity_x
    end

    if math.random(0, 1) == 0 then
        ball_velocity_y = -ball_velocity_y
    end
end

function update(dt)
    update_sounds(dt) -- Release sounds even on the winner screen.
    if winner then
        if sugarbee.btnp("b", 0) or sugarbee.btnp("b", 1) then
            left_score = 0
            right_score = 0
            winner = nil

            paddle_y = (screen_height - paddle_height) / 2
            right_paddle_y = paddle_y

            ball_x = (screen_width - ball_size) / 2
            ball_y = (screen_height - ball_size) / 2

            if math.random(0, 1) == 0 then
                ball_velocity_x = -ball_velocity_x
            end

            if math.random(0, 1) == 0 then
                ball_velocity_y = -ball_velocity_y
            end
        end

        return
    end

    if sugarbee.btn("up", 0) then
        paddle_y = paddle_y - paddle_speed * dt
    end

    if sugarbee.btn("down", 0) then
        paddle_y = paddle_y + paddle_speed * dt
    end

    -- move right paddle using player 2
    if sugarbee.btn("up", 1) then
        right_paddle_y = right_paddle_y - paddle_speed * dt
    end

    if sugarbee.btn("down", 1) then
        right_paddle_y = right_paddle_y + paddle_speed * dt
    end

    right_paddle_y = math.max(
        0,
        math.min(screen_height - paddle_height, right_paddle_y)
    )

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
        hit_sound(1, 330)
    elseif ball_y <= 0 then
        ball_y = 0
        ball_velocity_y = -ball_velocity_y
        hit_sound(1, 330)
    end

    -- screen boundary check - horizontal
    local point_scored = false

    if ball_x + ball_size >= screen_width then
        left_score = left_score + 1
        ball_velocity_x = -math.abs(ball_velocity_x)
        point_scored = true
    elseif ball_x <= 0 then
        right_score = right_score + 1
        ball_velocity_x = math.abs(ball_velocity_x)
        point_scored = true
    end

    -- start the next rally after either player scores
    if point_scored then
        ball_x = (screen_width - ball_size) / 2
        ball_y = (screen_height - ball_size) / 2

        if math.random(0, 1) == 0 then
            ball_velocity_y = -ball_velocity_y
        end

        if left_score >= winning_score then
            winner = "LEFT"
        elseif right_score >= winning_score then
            winner = "RIGHT"
        end

        return
    end

    -- detect bounce off left paddle
    if ball_velocity_x < 0
        and ball_x < paddle_x + paddle_width
        and ball_x + ball_size > paddle_x
        and ball_y < paddle_y + paddle_height
        and ball_y + ball_size > paddle_y then

        ball_x = paddle_x + paddle_width
        ball_velocity_x = -ball_velocity_x
        hit_sound(2, 660)
    end

    -- detect bounce off right paddle
    if ball_velocity_x > 0
        and ball_x < right_paddle_x + paddle_width
        and ball_x + ball_size > right_paddle_x
        and ball_y < right_paddle_y + paddle_height
        and ball_y + ball_size > right_paddle_y then

        ball_x = right_paddle_x - ball_size
        ball_velocity_x = -ball_velocity_x
        hit_sound(2, 660)
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
    sugarbee.rect(
        right_paddle_x, math.floor(right_paddle_y),
        paddle_width, paddle_height,
        80, 220, 160, true
    )

    -- draw ball
    sugarbee.rect(
        math.floor(ball_x), math.floor(ball_y),
        ball_size, ball_size,
        240, 230, 190, true
    )

    sugarbee.print("PONG LAB - PADDLE TEST", 160, 30)
    sugarbee.print("MOVE: UP/DOWN OR W/S", 160, 60)
    sugarbee.print("LEFT: " .. left_score .. "   RIGHT: " .. right_score, 160, 90)

    if winner then
        sugarbee.print(
            winner .. " WINS! PRESS B TO RESTART",
            160, 120
        )
    end
end

function shutdown()
    sugarbee.synth.stop(1)
    sugarbee.synth.stop(2)
end
