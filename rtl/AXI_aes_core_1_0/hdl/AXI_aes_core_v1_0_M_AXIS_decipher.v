
`timescale 1 ns / 1 ps

    module AXI_aes_core_v1_0_M_AXIS_decipher #
    (
        // Users to add parameters here

        // User parameters ends
        // Do not modify the parameters beyond this line

        // Width of S_AXIS address bus. The slave accepts the read and write addresses of width C_M_AXIS_TDATA_WIDTH.
        parameter integer C_M_AXIS_TDATA_WIDTH    = 128
    )
    (
        // Users to add ports here
        output reg busy,
        input wire data_valid,
        input wire data_last,
        input wire [C_M_AXIS_TDATA_WIDTH-1 : 0] data,
        // User ports ends
        // Do not modify the ports beyond this line

        // Global ports
        input wire  M_AXIS_ACLK,
        // 
        input wire  M_AXIS_ARESETN,
        // Master Stream Ports. TVALID indicates that the master is driving a valid transfer, A transfer takes place when both TVALID and TREADY are asserted. 
        output reg  M_AXIS_TVALID,
        // TDATA is the primary payload that is used to provide the data that is passing across the interface from the master.
        output reg [C_M_AXIS_TDATA_WIDTH-1 : 0] M_AXIS_TDATA,
        // TSTRB is the byte qualifier that indicates whether the content of the associated byte of TDATA is processed as a data byte or a position byte.
        output wire [(C_M_AXIS_TDATA_WIDTH/8)-1 : 0] M_AXIS_TSTRB,
        // TLAST indicates the boundary of a packet.
        output reg  M_AXIS_TLAST,
        // TREADY indicates that the slave can accept a transfer in the current cycle.
        input wire  M_AXIS_TREADY
    );
    
    reg [1:0] state = 0;

    localparam S0 = 0;
    localparam S1 = 1;
    localparam S2 = 2;

    assign M_AXIS_TSTRB = {(C_M_AXIS_TDATA_WIDTH/8){1'b1}};

    always @(posedge M_AXIS_ACLK) begin
        if (!M_AXIS_ARESETN) 
        begin
            M_AXIS_TVALID <= 0;
            M_AXIS_TDATA <= 0;
            M_AXIS_TLAST <= 0;
            busy <= 0;

            state <= S0;
        end
        else begin
            case(state)
                S0:     begin
                            M_AXIS_TVALID <= 0;
                            M_AXIS_TLAST <= 0;
                            busy <= !M_AXIS_TREADY;

                            if (data_valid && M_AXIS_TREADY) begin
                                M_AXIS_TDATA <= data;
                                M_AXIS_TLAST <= data_last;

                                state <= S1;
                            end else if (data_valid) begin
                                M_AXIS_TDATA <= data;
                                M_AXIS_TLAST <= data_last;

                                state <= S2;
                            end else begin
                                state <= S0;
                            end
                        end
                S1:     begin
                            M_AXIS_TVALID <= 1;
                            busy <= 1;

                            state <= S0;
                        end
                S2:     begin
                            busy <= 1;

                            if (M_AXIS_TREADY) begin
                                state <= S1;
                            end else begin
                                state <= S2;
                            end
                        end
                default:begin
                            M_AXIS_TVALID <= 0;
                            M_AXIS_TDATA <= 0;
                            M_AXIS_TLAST <= 0;
                            busy <= 0;

                            state <= S0;
                        end
            endcase
        end
    end

    endmodule
