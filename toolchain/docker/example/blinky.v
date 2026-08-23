module blinky #(
    parameter COUNTER_BITS = 24,
    parameter LED_BIT = 23
) (
    input  wire clk25,
    output wire led
);

    reg [COUNTER_BITS-1:0] counter = {COUNTER_BITS{1'b0}};

    always @(posedge clk25) begin
        counter <= counter + 1'b1;
    end

    assign led = counter[LED_BIT];

endmodule
