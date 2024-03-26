module tb_SPI_master_v1_0();
    wire sclk;
    wire mosi;
    wire miso;
    wire ss;
    
    reg axis_aclk = 0;
    reg axis_aresetn = 0;
    wire s_axis_tready;
    reg [31:0] s_axis_tdata=0;
    reg s_axis_tstrb=0;
    reg s_axis_tlast=0;
    reg s_axis_tvalid=0; 

    wire m_axis_tvalid;
    wire [31:0] m_axis_tdata;
    wire m_axis_tstrb;
    wire m_axis_tlast;
    reg m_axis_tready = 1;
    
    localparam WIDTH = 8;
    localparam CLOCK_DIVISOR = 2;
    
    reg [WIDTH-1 : 0] miso_reg = 0;

    SPI_master_v1_0 # (
        .WIDTH(WIDTH),
        .CLOCK_DIVISOR(CLOCK_DIVISOR),
        .C_S_AXIS_TDATA_WIDTH(32),
        .C_M_AXIS_TDATA_WIDTH(32)
    ) SPI_master_v1_0_inst (
        .sclk(sclk),
        .mosi(mosi),
        .miso(miso),
        .ss(ss),
        
        .AXIS_ACLK(axis_aclk),
        .AXIS_ARESETN(axis_aresetn),
        
        .S_AXIS_TREADY(s_axis_tready),
        .S_AXIS_TDATA(s_axis_tdata),
        .S_AXIS_TSTRB(s_axis_tstrb),
        .S_AXIS_TLAST(s_axis_tlast),
        .S_AXIS_TVALID(s_axis_tvalid),
        
        .M_AXIS_TVALID(m_axis_tvalid),
        .M_AXIS_TDATA(m_axis_tdata),
        .M_AXIS_TSTRB(m_axis_tstrb),
        .M_AXIS_TLAST(m_axis_tlast),
        .M_AXIS_TREADY(m_axis_tready)
    );

    always @ (posedge sclk) begin
    if (!s_axis_tready)
        miso_reg <= miso_reg << 1;
    end
    
    assign miso = miso_reg[WIDTH-1];

    always #1 axis_aclk = ~axis_aclk;

    initial begin
        axis_aresetn = 0;
        repeat(5) @ (posedge axis_aclk);
        axis_aresetn = 1;
        
        @ (posedge axis_aclk) begin
            s_axis_tdata = 8'hAA;
            s_axis_tvalid = 1;
            s_axis_tlast = 1;
        end
        while(!s_axis_tready) @ (posedge axis_aclk);
        miso_reg = 8'hAB;

        @ (posedge axis_aclk) begin
            s_axis_tdata = 8'hBB;
            s_axis_tvalid = 1;
            s_axis_tlast = 0;
        end
        while(!s_axis_tready) @ (posedge axis_aclk);
        miso_reg = 8'hCC;

        @ (posedge axis_aclk) begin
            s_axis_tdata = 8'h12;
            s_axis_tvalid = 1;
            s_axis_tlast = 1;
        end
        while(!s_axis_tready) @ (posedge axis_aclk);
        miso_reg = 8'hDD;

        @ (posedge axis_aclk) begin
            s_axis_tvalid = 0;
        end
    end
endmodule
