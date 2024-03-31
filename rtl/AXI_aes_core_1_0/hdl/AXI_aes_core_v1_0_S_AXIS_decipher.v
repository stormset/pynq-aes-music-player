
`timescale 1 ns / 1 ps

    module AXI_aes_core_v1_0_S_AXIS_decipher #
    (
        // Users to add parameters here

        // User parameters ends
        // Do not modify the parameters beyond this line

        // AXI4Stream sink: Data Width
        parameter integer C_S_AXIS_TDATA_WIDTH    = 128,
        parameter integer C_S_AXI_DATA_WIDTH    = 32
    )
    (
        // Users to add ports here
        output reg data_valid,
        output reg data_last,
        output reg [C_S_AXIS_TDATA_WIDTH-1 : 0] data,
        output reg key_valid,
        output reg [C_S_AXIS_TDATA_WIDTH-1 : 0] key,

        output reg decipher_discard_result,
        input wire [C_S_AXI_DATA_WIDTH-1 : 0] decipher_ctrl_reg,
        input wire decipher_kdone,
        input wire decipher_finished,
        input wire decipher_master_busy,
        // User ports ends
        // Do not modify the ports beyond this line

        // AXI4Stream sink: Clock
        input wire  S_AXIS_ACLK,
        // AXI4Stream sink: Reset
        input wire  S_AXIS_ARESETN,
        // Ready to accept data in
        output reg  S_AXIS_TREADY,
        // Data in
        input wire [C_S_AXIS_TDATA_WIDTH-1 : 0] S_AXIS_TDATA,
        // Byte qualifier
        input wire [(C_S_AXIS_TDATA_WIDTH/8)-1 : 0] S_AXIS_TSTRB,
        // Indicates boundary of last packet
        input wire  S_AXIS_TLAST,
        // Data is in valid
        input wire  S_AXIS_TVALID
    );
    
    localparam S0 = 0;
    localparam S1 = 1;
    localparam S2 = 2;

    wire setup_needed;
    reg setup_done_reg = 1;
    reg [1:0] state = 0;

    assign setup_needed = decipher_ctrl_reg[0] || !setup_done_reg;

    always @(posedge S_AXIS_ACLK) begin
        if (!S_AXIS_ARESETN) 
        begin
            S_AXIS_TREADY <= 0;
            data_valid <= 0;
            data_last <= 0;
            setup_done_reg <= 1;
            key_valid <= 1;
            key <= 0;
            decipher_discard_result <= 0;

            state <= S0;
        end
        else begin
            case(state)
                S0:     begin
                            /* We could introduce some pipeline registers to the master interface
                               to avoid throttling here (!decipher_master_busy) */
                            S_AXIS_TREADY <= setup_needed || !decipher_master_busy;
                            data_valid <= 0;
                            data_last <= 0;
                            key_valid <= 1;
                            decipher_discard_result <= 0;

                            state <= S0;
                            if (setup_needed) begin
                                if (S_AXIS_TVALID == 1) begin
                                    setup_done_reg <= 1;
									key_valid <= 0;
                                    key <= S_AXIS_TDATA;

                                    state <= S2;
                                end else begin
                                    setup_done_reg <= 0;
                                end        
                            end else if (!decipher_master_busy && S_AXIS_TVALID == 1) begin
                                data <= S_AXIS_TDATA;
                                data_last <= S_AXIS_TLAST;
                                
                                state <= S1;
                            end
                        end
                S1:     begin
                            S_AXIS_TREADY <= setup_needed;
                            key_valid <= 1;
                                                                                                                                                                                                      
                            if (setup_needed) begin
                                data_valid <= 0;
                                decipher_discard_result <= 1;

                                if (decipher_finished == 0) begin
                                    setup_done_reg <= 0;
                                    S_AXIS_TREADY <= 0;

                                    state <= S1;
                                end else begin
                                    if (S_AXIS_TVALID == 1) begin
                                        setup_done_reg <= 1;
										key_valid <= 0;
                                        key <= S_AXIS_TDATA;

                                        state <= S2;
                                    end else begin
                                        setup_done_reg <= 0;

                                        state <= S0;
                                    end
                                end        
                            end else begin
                                data_valid <= 1;
                                decipher_discard_result <= 0;

                                if (decipher_finished == 0) begin
                                    state <= S1;
                                end else begin
                                    state <= S0;
                                end
                            end

                        end
                S2:     begin
                            S_AXIS_TREADY <= 0;
                            key_valid <= 0;

                            if (decipher_kdone) begin
                                state <= S0;
								key_valid <= 1;
                            end else begin
                                state <= S2;
                            end


                        end
                default:begin
                            S_AXIS_TREADY <= 0;
                            data_valid <= 0;
                            data_last <= 0;
                            setup_done_reg <= 1;
                            decipher_discard_result <= 0;

                            state <= S0;
                        end
            endcase
        end
    end
    
    endmodule
