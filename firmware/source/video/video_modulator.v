//`default_nettype none

module video_modulator(
    input  wire        clk,

    input  wire  [3:0] r,
    input  wire  [3:0] g,
    input  wire  [3:0] b,
    input  wire        color_burst,
    input  wire        active,
    input  wire        sync_n_in,

    output reg   [5:0] luma,
    output reg   [5:0] chroma);

    parameter Y_R = 27; // 38; //  0.299
    parameter Y_G = 53; // 75; //  0.587
    parameter Y_B = 10; // 14; //  0.114

    parameter I_R =  76; //  0.5959
    parameter I_G = -35; // -0.2746
    parameter I_B = -41; // -0.3213

    parameter Q_R =  27; //  0.2115
    parameter Q_G = -66; // -0.5227
    parameter Q_B =  40; //  0.3112

    wire signed [4:0] r_s = color_burst ? 5'd9 : {1'b0, r};
    wire signed [4:0] g_s = color_burst ? 5'd9 : {1'b0, g};
    wire signed [4:0] b_s = color_burst ? 5'd0 : {1'b0, b};

    // Isolate the timing/control decode from the color-matrix arithmetic.  All
    // associated controls are delayed with the samples so luma and chroma stay
    // aligned; this adds one clock of modulator latency.
    reg signed [4:0] r_s_r;
    reg signed [4:0] g_s_r;
    reg signed [4:0] b_s_r;
    reg              color_burst_r;
    reg              active_r;
    reg              sync_n_in_r;

    always @(posedge clk) begin
        r_s_r         <= r_s;
        g_s_r         <= g_s;
        b_s_r         <= b_s;
        color_burst_r <= color_burst;
        active_r      <= active;
        sync_n_in_r   <= sync_n_in;
    end

    reg signed [11:0] y_s;
    reg signed [11:0] i_s;
    reg signed [11:0] q_s;

    wire signed [11:0] r_ext = {{7{r_s_r[4]}}, r_s_r};
    wire signed [11:0] g_ext = {{7{g_s_r[4]}}, g_s_r};
    wire signed [11:0] b_ext = {{7{b_s_r[4]}}, b_s_r};

    // The color matrix has fixed coefficients.  Expressing it as shifts and
    // adds prevents these small constant products from consuming the UP5K's
    // scarce DSP blocks, which are needed by the carrier modulation below.
    wire signed [11:0] y_matrix =
        (r_ext <<< 4) + (r_ext <<< 3) + (r_ext <<< 1) + r_ext +
        (g_ext <<< 5) + (g_ext <<< 4) + (g_ext <<< 2) + g_ext +
        (b_ext <<< 3) + (b_ext <<< 1);

    wire signed [11:0] i_matrix =
        (r_ext <<< 6) + (r_ext <<< 3) + (r_ext <<< 2) -
        (g_ext <<< 5) - (g_ext <<< 1) - g_ext -
        (b_ext <<< 5) - (b_ext <<< 3) - b_ext;

    wire signed [11:0] q_matrix =
        (r_ext <<< 5) - (r_ext <<< 2) - r_ext -
        (g_ext <<< 6) - (g_ext <<< 1) +
        (b_ext <<< 5) + (b_ext <<< 3);

    always @(posedge clk) begin
        y_s <= (sync_n_in_r == 0) ? 12'd0 : 12'd544;
        i_s <= 0;
        q_s <= 0;

        if (active_r) begin
            y_s <= y_matrix + (128 + 512);
        end

        if (active_r || color_burst_r) begin
            i_s <= i_matrix;
            q_s <= q_matrix;
        end
    end

    // Color burst frequency: 315/88 MHz = 3579545 Hz
    reg  [23:0] phase_accum_r = 0;
    always @(posedge clk) phase_accum_r <= phase_accum_r + 24'd2402192;

    // Match the extra color-sample pipeline stage above so the carrier phase
    // relative to sync, burst, and active pixels is unchanged.
    reg [8:0] phase_r;
    always @(posedge clk) phase_r <= phase_accum_r[23:15];

    wire [7:0] sinval;
    video_modulator_sinlut sinlut(
        .clk(clk),
        .phase(phase_r),
        .value(sinval));

    wire [7:0] cosval;
    video_modulator_coslut coslut(
        .clk(clk),
        .phase(phase_r),
        .value(cosval));

    wire signed [7:0] sinval_s = sinval;
    wire signed [7:0] cosval_s = cosval;

    wire signed [7:0] i8_s = i_s[11:4];
    wire signed [7:0] q8_s = q_s[11:4];

    reg         [7:0] lum;
    reg signed [13:0] chroma_s;

    always @(posedge clk) begin
        if (y_s < 0)
            lum <= 0;
        else if (y_s >= 2047)
            lum <= 255;
        else
            lum <= y_s[10:3];

        chroma_s <= (cosval_s * i8_s) + (sinval_s * q8_s);
    end

    always @(posedge clk) begin
        luma   <= lum[7:2];
        chroma <= chroma_s[13:8] + 6'd32;
    end

endmodule
