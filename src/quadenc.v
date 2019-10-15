module quadenc(clk, quadA, quadB, count);
input clk, quadA, quadB;
output [31:0] count;

reg [2:0] quadA_delayed, quadB_delayed;
always @(posedge clk) quadA_delayed <= {quadA_delayed[1:0], quadA};
always @(posedge clk) quadB_delayed <= {quadB_delayed[1:0], quadB};

wire enable = quadA_delayed[1] ^ quadA_delayed[2] ^ quadB_delayed[1] ^ quadB_delayed[2];
wire direction = quadA_delayed[1] ^ quadB_delayed[2];

reg [31:0] count;
always @(posedge clk) begin
    if (enable) begin
        if (direction)
            count <= count + 1;
        else
            count <= count - 1;
    end
end

endmodule
