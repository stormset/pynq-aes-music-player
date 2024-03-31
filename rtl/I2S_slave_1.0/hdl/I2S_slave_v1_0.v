
`timescale 1 ns / 1 ps

    module I2S_slave_v1_0 #
    (
        // Users to add parameters here

        // User parameters ends
        // Do not modify the parameters beyond this line


        // Parameters of Axi Slave Bus Interface S_AXIS
        parameter integer C_S_AXIS_TDATA_WIDTH	= 32
    )
    (
        // Users to add parameters here
        input bclk,
        input lrclk,
        output sdata,
        // User ports ends
        // Do not modify the ports beyond this line

        // AXI4Stream sink: Clock
        input wire  S_AXIS_ACLK,
        // AXI4Stream sink: Reset
        input wire  S_AXIS_ARESETN,
        // Ready to accept data in
        output reg  S_AXIS_TREADY = 0,
        // Data in
        input wire [C_S_AXIS_TDATA_WIDTH-1 : 0] S_AXIS_TDATA,
        // Byte qualifier
        input wire  S_AXIS_TLAST,
        // Data is in valid
        input wire  S_AXIS_TVALID
    );
            
    reg [1:0] bclk_ctrl = 0;
    always @(posedge S_AXIS_ACLK) begin
        bclk_ctrl <= {bclk_ctrl, bclk};
    end
    wire bclk_rise = bclk_ctrl == 2'b01;
    wire bclk_fall = bclk_ctrl == 2'b10;
    reg busy = 0;

    reg [1:0] lrclk_buff = 0;
    always @(posedge bclk) begin
        lrclk_buff <= {lrclk_buff, lrclk};
    end
    wire lrclkd = lrclk_buff[0]; // channel (left / NOT(right) or the opposite depending on the config register of the DAC)
    wire lrclkp = ^lrclk_buff;   // lrclk edge detect

    reg [0:C_S_AXIS_TDATA_WIDTH - 1] data_buf = 0;
        
    always @(posedge S_AXIS_ACLK) begin
        if (!S_AXIS_ARESETN) begin
            busy <= 0;
        end else begin
            if (S_AXIS_TREADY && S_AXIS_TVALID) begin
                busy <= 1;
                data_buf <= S_AXIS_TDATA;
            end
            if (bclk_fall) begin
                if (lrclkp) begin
                    if(lrclk_buff == 2'b01) begin
                        data_buf <= data_buf << 1;
                    end
                end else begin
                    busy <= 0;
                    data_buf <= data_buf << 1;
                end
            end
        end
    end

    assign sdata = data_buf[0];

    always @(posedge S_AXIS_ACLK) begin
        if (!S_AXIS_ARESETN) begin
            S_AXIS_TREADY <= 0;
        end
        else if (S_AXIS_TREADY && S_AXIS_TVALID)
            S_AXIS_TREADY <= 0;
        else if ((lrclkp && !busy && lrclk_buff != 2'b01)) begin
            S_AXIS_TREADY <= 1;
        end
    end

    endmodule
