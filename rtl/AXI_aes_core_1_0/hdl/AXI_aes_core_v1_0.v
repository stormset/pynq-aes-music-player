
`timescale 1 ns / 1 ps

    module AXI_aes_core_v1_0 #
    (
        // Users to add parameters here

        // User parameters ends
        // Do not modify the parameters beyond this line


        // Parameters of Axi Slave Bus Interface S_AXI
        parameter integer C_S_AXI_DATA_WIDTH = 32,
        parameter integer C_S_AXI_ADDR_WIDTH = 4,

        // Parameters of Axi Slave Bus Interface S_AXIS_cipher
        parameter integer C_S_AXIS_cipher_TDATA_WIDTH = 128,

        // Parameters of Axi Slave Bus Interface S_AXIS_decipher
        parameter integer C_S_AXIS_decipher_TDATA_WIDTH = 128,

        // Parameters of Axi Master Bus Interface M_AXIS_cipher
        parameter integer C_M_AXIS_cipher_TDATA_WIDTH = 128,

        // Parameters of Axi Master Bus Interface M_AXIS_decipher
        parameter integer C_M_AXIS_decipher_TDATA_WIDTH = 128
    )
    (
        // Users to add ports here
        output wire cipher_fifo_rst,
        output wire decipher_fifo_rst,
        // User ports ends
        // Do not modify the ports beyond this line


        // Ports of Axi Slave Bus Interface S_AXI
        input wire  s_axi_aclk,
        input wire  s_axi_aresetn,
        input wire [C_S_AXI_ADDR_WIDTH-1 : 0] s_axi_awaddr,
        input wire [2 : 0] s_axi_awprot,
        input wire  s_axi_awvalid,
        output wire  s_axi_awready,
        input wire [C_S_AXI_DATA_WIDTH-1 : 0] s_axi_wdata,
        input wire [(C_S_AXI_DATA_WIDTH/8)-1 : 0] s_axi_wstrb,
        input wire  s_axi_wvalid,
        output wire  s_axi_wready,
        output wire [1 : 0] s_axi_bresp,
        output wire  s_axi_bvalid,
        input wire  s_axi_bready,
        input wire [C_S_AXI_ADDR_WIDTH-1 : 0] s_axi_araddr,
        input wire [2 : 0] s_axi_arprot,
        input wire  s_axi_arvalid,
        output wire  s_axi_arready,
        output wire [C_S_AXI_DATA_WIDTH-1 : 0] s_axi_rdata,
        output wire [1 : 0] s_axi_rresp,
        output wire  s_axi_rvalid,
        input wire  s_axi_rready,

        // Ports of Axi Slave Bus Interface S_AXIS_cipher
        input wire  s_axis_cipher_aclk,
        input wire  s_axis_cipher_aresetn,
        output wire  s_axis_cipher_tready,
        input wire [C_S_AXIS_cipher_TDATA_WIDTH-1 : 0] s_axis_cipher_tdata,
        input wire [(C_S_AXIS_cipher_TDATA_WIDTH/8)-1 : 0] s_axis_cipher_tstrb,
        input wire  s_axis_cipher_tlast,
        input wire  s_axis_cipher_tvalid,

        // Ports of Axi Slave Bus Interface S_AXIS_decipher
        input wire  s_axis_decipher_aclk,
        input wire  s_axis_decipher_aresetn,
        output wire  s_axis_decipher_tready,
        input wire [C_S_AXIS_decipher_TDATA_WIDTH-1 : 0] s_axis_decipher_tdata,
        input wire [(C_S_AXIS_decipher_TDATA_WIDTH/8)-1 : 0] s_axis_decipher_tstrb,
        input wire  s_axis_decipher_tlast,
        input wire  s_axis_decipher_tvalid,

        // Ports of Axi Master Bus Interface M_AXIS_cipher
        input wire  m_axis_cipher_aclk,
        input wire  m_axis_cipher_aresetn,
        output wire  m_axis_cipher_tvalid,
        output wire [C_M_AXIS_cipher_TDATA_WIDTH-1 : 0] m_axis_cipher_tdata,
        output wire [(C_M_AXIS_cipher_TDATA_WIDTH/8)-1 : 0] m_axis_cipher_tstrb,
        output wire  m_axis_cipher_tlast,
        input wire  m_axis_cipher_tready,

        // Ports of Axi Master Bus Interface M_AXIS_decipher
        input wire  m_axis_decipher_aclk,
        input wire  m_axis_decipher_aresetn,
        output wire  m_axis_decipher_tvalid,
        output wire [C_M_AXIS_decipher_TDATA_WIDTH-1 : 0] m_axis_decipher_tdata,
        output wire [(C_M_AXIS_decipher_TDATA_WIDTH/8)-1 : 0] m_axis_decipher_tstrb,
        output wire  m_axis_decipher_tlast,
        input wire  m_axis_decipher_tready
    );

/// cipher signals
    wire cipher_done, cipher_master_busy, cipher_last, cipher_text_in_valid, cipher_key_valid, cipher_text_out_valid, cipher_discard_result;
    wire [C_S_AXIS_cipher_TDATA_WIDTH-1 : 0] cipher_key, cipher_text_in, cipher_text_out;

    wire [C_S_AXI_DATA_WIDTH-1 : 0] cipher_ctrl_reg;
    reg [C_S_AXI_DATA_WIDTH-1 : 0] cipher_status_reg;

    wire cipher_start_pulse;
    reg cipher_start, cipher_start_prev;

/// decipher signals
    wire cipher_bsy, decipher_bsy;
    wire decipher_done, decipher_kdone, decipher_master_busy, decipher_last, decipher_text_in_valid, decipher_key_valid, decipher_text_out_valid, decipher_discard_result;
    wire [C_S_AXIS_cipher_TDATA_WIDTH-1 : 0] decipher_key, decipher_text_in, decipher_text_out;

    wire [C_S_AXI_DATA_WIDTH-1 : 0] decipher_ctrl_reg;
    reg [C_S_AXI_DATA_WIDTH-1 : 0] decipher_status_reg;

    wire decipher_start_pulse, decipher_kload_pulse;
    reg decipher_start, decipher_start_prev, decipher_kload, decipher_kload_prev;

    wire [C_S_AXI_DATA_WIDTH-1 : 0] cipher_reg, decipher_reg;
// Instantiation of Axi Bus Interface S_AXI
    AXI_aes_core_v1_0_S_AXI # ( 
        .C_S_AXI_DATA_WIDTH(C_S_AXI_DATA_WIDTH),
        .C_S_AXI_ADDR_WIDTH(C_S_AXI_ADDR_WIDTH)
    ) AXI_aes_core_v1_0_S_AXI_inst (
        .cipher_reg(cipher_reg),
        .decipher_reg(decipher_reg),
        .cipher_ctrl_reg(cipher_ctrl_reg),
        .cipher_status_reg(cipher_status_reg),
        .decipher_ctrl_reg(decipher_ctrl_reg),
        .decipher_status_reg(decipher_status_reg),
        .S_AXI_ACLK(s_axi_aclk),
        .S_AXI_ARESETN(s_axi_aresetn),
        .S_AXI_AWADDR(s_axi_awaddr),
        .S_AXI_AWPROT(s_axi_awprot),
        .S_AXI_AWVALID(s_axi_awvalid),
        .S_AXI_AWREADY(s_axi_awready),
        .S_AXI_WDATA(s_axi_wdata),
        .S_AXI_WSTRB(s_axi_wstrb),
        .S_AXI_WVALID(s_axi_wvalid),
        .S_AXI_WREADY(s_axi_wready),
        .S_AXI_BRESP(s_axi_bresp),
        .S_AXI_BVALID(s_axi_bvalid),
        .S_AXI_BREADY(s_axi_bready),
        .S_AXI_ARADDR(s_axi_araddr),
        .S_AXI_ARPROT(s_axi_arprot),
        .S_AXI_ARVALID(s_axi_arvalid),
        .S_AXI_ARREADY(s_axi_arready),
        .S_AXI_RDATA(s_axi_rdata),
        .S_AXI_RRESP(s_axi_rresp),
        .S_AXI_RVALID(s_axi_rvalid),
        .S_AXI_RREADY(s_axi_rready)
    );

// Instantiation of Axi Bus Interface S_AXIS_cipher
    AXI_aes_core_v1_0_S_AXIS_cipher # ( 
        .C_S_AXIS_TDATA_WIDTH(C_S_AXIS_cipher_TDATA_WIDTH),
        .C_S_AXI_DATA_WIDTH(C_S_AXI_DATA_WIDTH)
    ) AXI_aes_core_v1_0_S_AXIS_cipher_inst (
        .data_valid(cipher_text_in_valid),
        .data_last(cipher_last),
        .data(cipher_text_in),
        .key_valid(cipher_key_valid),
        .key(cipher_key),
        .cipher_discard_result(cipher_discard_result),
        .cipher_ctrl_reg(cipher_ctrl_reg),
        .cipher_finished(cipher_done),
        .cipher_master_busy(cipher_bsy),
        .S_AXIS_ACLK(s_axis_cipher_aclk),
        .S_AXIS_ARESETN(s_axis_cipher_aresetn),
        .S_AXIS_TREADY(s_axis_cipher_tready),
        .S_AXIS_TDATA(s_axis_cipher_tdata),
        .S_AXIS_TSTRB(s_axis_cipher_tstrb),
        .S_AXIS_TLAST(s_axis_cipher_tlast),
        .S_AXIS_TVALID(s_axis_cipher_tvalid)
    );

// Instantiation of Axi Bus Interface S_AXIS_decipher
    AXI_aes_core_v1_0_S_AXIS_decipher # ( 
        .C_S_AXIS_TDATA_WIDTH(C_S_AXIS_decipher_TDATA_WIDTH),
        .C_S_AXI_DATA_WIDTH(C_S_AXI_DATA_WIDTH)
    ) AXI_aes_core_v1_0_S_AXIS_decipher_inst (
        .data_valid(decipher_text_in_valid),
        .data_last(decipher_last),
        .data(decipher_text_in),
        .key_valid(decipher_key_valid),
        .key(decipher_key),
        .decipher_discard_result(decipher_discard_result),
        .decipher_ctrl_reg(decipher_ctrl_reg),
        .decipher_kdone(decipher_kdone),
        .decipher_finished(decipher_done),
        .decipher_master_busy(decipher_bsy),
        .S_AXIS_ACLK(s_axis_decipher_aclk),
        .S_AXIS_ARESETN(s_axis_decipher_aresetn),
        .S_AXIS_TREADY(s_axis_decipher_tready),
        .S_AXIS_TDATA(s_axis_decipher_tdata),
        .S_AXIS_TSTRB(s_axis_decipher_tstrb),
        .S_AXIS_TLAST(s_axis_decipher_tlast),
        .S_AXIS_TVALID(s_axis_decipher_tvalid)
    );

// Instantiation of Axi Bus Interface M_AXIS_cipher
    AXI_aes_core_v1_0_M_AXIS_cipher # ( 
        .C_M_AXIS_TDATA_WIDTH(C_M_AXIS_cipher_TDATA_WIDTH)
    ) AXI_aes_core_v1_0_M_AXIS_cipher_inst (
        .busy(cipher_master_busy),
        .data_valid(cipher_text_out_valid),
        .data_last(cipher_last),
        .data(cipher_text_out),
        .M_AXIS_ACLK(m_axis_cipher_aclk),
        .M_AXIS_ARESETN(m_axis_cipher_aresetn),
        .M_AXIS_TVALID(m_axis_cipher_tvalid),
        .M_AXIS_TDATA(m_axis_cipher_tdata),
        .M_AXIS_TSTRB(m_axis_cipher_tstrb),
        .M_AXIS_TLAST(m_axis_cipher_tlast),
        .M_AXIS_TREADY(m_axis_cipher_tready)
    );

// Instantiation of Axi Bus Interface M_AXIS_decipher
    AXI_aes_core_v1_0_M_AXIS_decipher # ( 
        .C_M_AXIS_TDATA_WIDTH(C_M_AXIS_decipher_TDATA_WIDTH)
    ) AXI_aes_core_v1_0_M_AXIS_decipher_inst (
        .busy(decipher_master_busy),
        .data_valid(decipher_text_out_valid),
        .data_last(decipher_last),
        .data(decipher_text_out),
        .M_AXIS_ACLK(m_axis_decipher_aclk),
        .M_AXIS_ARESETN(m_axis_decipher_aresetn),
        .M_AXIS_TVALID(m_axis_decipher_tvalid),
        .M_AXIS_TDATA(m_axis_decipher_tdata),
        .M_AXIS_TSTRB(m_axis_decipher_tstrb),
        .M_AXIS_TLAST(m_axis_decipher_tlast),
        .M_AXIS_TREADY(m_axis_decipher_tready)
    );

    // Add user logic here

// AES cipher logic
    assign cipher_text_out_valid = cipher_done; //&& !cipher_discard_result;
    assign cipher_fifo_rst = ~cipher_reg[0] & m_axis_cipher_aresetn;

    // cipher status register
    always @(*) 
        cipher_status_reg = {31'h0, cipher_key_valid};

    // generate start pulse for cipher when cipher_text_in_valid is asserted
    always @( posedge s_axi_aclk )
    begin
        if ( s_axi_aresetn == 1'b0 )
        begin
            cipher_start           <= 0;
            cipher_start_prev      <= 0;
        end else begin
            cipher_start           <= cipher_text_in_valid;
            cipher_start_prev      <= cipher_start;
        end
    end    
    assign cipher_start_pulse = ~cipher_start_prev & cipher_start;

    aes_cipher_top aes_cipher (
        .clk(s_axis_cipher_aclk),
        .rst(s_axis_cipher_aresetn),
        .ld(cipher_start_pulse),
        .done(cipher_done),
        .key(cipher_key),
        .text_in(cipher_text_in),
        .text_out(cipher_text_out)
    );

// AES decipher logic
    assign decipher_text_out_valid = decipher_done; //&& !decipher_discard_result;
    assign decipher_fifo_rst = ~decipher_reg[0] & m_axis_decipher_aresetn;

    // decipher status register
    always @(*) 
        decipher_status_reg = {31'h0, decipher_key_valid};

    // generate start pulse for decipher when decipher_text_in_valid is asserted
    always @( posedge s_axi_aclk )
    begin
        if ( s_axi_aresetn == 1'b0 )
        begin
            decipher_start           <= 0;
            decipher_start_prev      <= 0;
        end else begin
            decipher_start           <= decipher_text_in_valid;
            decipher_start_prev      <= decipher_start;
        end
    end    
    assign decipher_start_pulse = ~decipher_start_prev & decipher_start;

    // generate key load pulse for decipher when decipher_key_valid is deasserted
    always @( posedge s_axi_aclk )
    begin
        if ( s_axi_aresetn == 1'b0 )
        begin
            decipher_kload           <= 0;
            decipher_kload_prev      <= 0;
        end else begin
            decipher_kload           <= ~decipher_key_valid;
            decipher_kload_prev      <= decipher_kload;
        end
    end    
    assign decipher_kload_pulse = ~decipher_kload_prev & decipher_kload;

    aes_inv_cipher_top aes_decipher(
        .clk(s_axis_decipher_aclk),
        .rst(s_axis_decipher_aresetn),
        .kld(decipher_kload_pulse),
        .ld(decipher_start_pulse),
        .done(decipher_done),
        .key(decipher_key),
        .kdone(decipher_kdone),
        .text_in(decipher_text_in),
        .text_out(decipher_text_out)
    );

    assign cipher_bsy = cipher_master_busy | ~cipher_fifo_rst;
    assign decipher_bsy = decipher_master_busy | ~decipher_fifo_rst;

    // User logic ends

    endmodule
