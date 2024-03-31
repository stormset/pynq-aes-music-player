`timescale 1ns / 1ps

    module tb_I2S_slave_v1_0();
        localparam WIDTH = 32;
        localparam BCLK_PERIOD = 20;

    
        reg bclk = 1;
        reg lrclk = 1;
        wire sdata;
        
        reg s_axis_aclk = 0;
        reg s_axis_aresetn;
        wire s_axis_tready;
        reg [WIDTH-1 : 0] s_axis_tdata = 0;
        reg s_axis_tlast = 0;
        reg s_axis_tvalid = 0;
        
    
        I2S_slave_v1_0 # (
            .C_S_AXIS_TDATA_WIDTH(WIDTH)
        ) I2S_slave_v1_0_inst (
            .bclk(bclk),
            .lrclk(lrclk),
            .sdata(sdata),
            .S_AXIS_ACLK(s_axis_aclk),
            .S_AXIS_ARESETN(s_axis_aresetn),
            .S_AXIS_TREADY(s_axis_tready),
            .S_AXIS_TDATA(s_axis_tdata),
            .S_AXIS_TLAST(s_axis_tlast),
            .S_AXIS_TVALID(s_axis_tvalid)
        );
    
        always #1 s_axis_aclk=~s_axis_aclk;
        always #(BCLK_PERIOD / 2) bclk=~bclk;
        always #(WIDTH * (BCLK_PERIOD / 2)) lrclk=~lrclk;
    
        initial begin
            s_axis_aresetn <= 0;
            #100;
            s_axis_aresetn <= 1;

            @ (posedge s_axis_aclk) s_axis_tdata = 32'hF0F0F0F0;s_axis_tvalid = 1;
            while (s_axis_tready==0) begin
                @ (posedge s_axis_aclk) ;
            end
            @ (posedge s_axis_aclk) s_axis_tdata = 32'h0A0A0A0A;s_axis_tvalid = 1;
            while (s_axis_tready==0) begin
                @ (posedge s_axis_aclk) ;
            end
            @ (posedge s_axis_aclk) s_axis_tdata = 32'h0B0B0B0B;s_axis_tvalid = 1;
            while (s_axis_tready==0) begin
                @ (posedge s_axis_aclk) ;
            end
            @ (posedge s_axis_aclk) s_axis_tdata = 32'h0C0C0C0C;s_axis_tvalid = 1;
            while (s_axis_tready==0) begin
                @ (posedge s_axis_aclk) ;
            end
        end
    
    endmodule
