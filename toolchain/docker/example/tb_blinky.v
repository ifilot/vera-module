`timescale 1ns/1ps

module tb_blinky;
    reg clk25 = 1'b0;
    wire led;

    always #20 clk25 = ~clk25;

    blinky #(
        .COUNTER_BITS(4),
        .LED_BIT(3)
    ) dut (
        .clk25(clk25),
        .led(led)
    );

    initial begin
        repeat (8) @(posedge clk25);
        #1;
        if (led !== 1'b1) begin
            $display("FAIL: LED did not become high");
            $finish(1);
        end

        repeat (8) @(posedge clk25);
        #1;
        if (led !== 1'b0) begin
            $display("FAIL: LED did not become low");
            $finish(1);
        end

        $display("PASS: blinky simulation");
        $finish(0);
    end
endmodule
