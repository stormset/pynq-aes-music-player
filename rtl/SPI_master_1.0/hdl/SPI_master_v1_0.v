`timescale 1 ns / 1 ps
    
    module SPI_master_v1_0 #
    (
        // Users to add parameters here
        parameter integer WIDTH    = 8,
        parameter integer CLOCK_DIVISOR = 4,
        // User parameters ends
        // Do not modify the parameters beyond this line


        // Parameters of Axi Slave Bus Interface S_AXIS
        parameter integer C_S_AXIS_TDATA_WIDTH = 32,

        // Parameters of Axi Master Bus Interface M_AXIS
        parameter integer C_M_AXIS_TDATA_WIDTH = 32
    )
    (
        // Users to add ports here
        output wire sclk,
        output reg mosi = 0,
        input wire miso,
        output wire ss,
        // User ports ends
        // Do not modify the ports beyond this line

        input wire  AXIS_ACLK,
        input wire  AXIS_ARESETN,
        
        // Ready to accept data in
        output wire  S_AXIS_TREADY,
        // Data in
        input wire [C_S_AXIS_TDATA_WIDTH-1 : 0] S_AXIS_TDATA,
        // Byte qualifier
        input wire [(C_S_AXIS_TDATA_WIDTH/8)-1 : 0] S_AXIS_TSTRB,
        // Indicates boundary of last packet
        input wire  S_AXIS_TLAST,
        // Data is in valid
        input wire  S_AXIS_TVALID,

        // Master Stream Ports. TVALID indicates that the master is driving a valid transfer, A transfer takes place when both TVALID and TREADY are asserted. 
        output reg  M_AXIS_TVALID,
        // TDATA is the primary payload that is used to provide the data that is passing across the interface from the master.
        output reg [C_M_AXIS_TDATA_WIDTH-1 : 0] M_AXIS_TDATA = 0,
        // TSTRB is the byte qualifier that indicates whether the content of the associated byte of TDATA is processed as a data byte or a position byte.
        output wire [(C_M_AXIS_TDATA_WIDTH/8)-1 : 0] M_AXIS_TSTRB,
        // TLAST indicates the boundary of a packet.
        output wire  M_AXIS_TLAST,
        // TREADY indicates that the slave can accept a transfer in the current cycle.
        input wire  M_AXIS_TREADY
    );

    reg [WIDTH-1 : 0] tx_buffer = 0;
    reg [WIDTH-1 : 0] rx_buffer = 0;
    
    reg spi_ready = 1;
    reg last = 0;
    reg last_buff = 0;
    reg [1:0] rx_full = 0;

    reg [$clog2(WIDTH) : 0] counter = 0;
    reg [$clog2(CLOCK_DIVISOR) : 0] clk_counter = 0;

    localparam S0 = 0;
    localparam S1 = 1;
    localparam S2 = 2;

    reg [1:0] state = S0;

    assign M_AXIS_TSTRB = {(C_M_AXIS_TDATA_WIDTH/8){1'b1}};
    assign M_AXIS_TLAST = 1;
    assign S_AXIS_TREADY = spi_ready;

    assign sclk = (state == S2);      // clock polarity = 0 (0 when idle)
    assign ss = (state == S0);        // slave select: active low
    
    always @(posedge AXIS_ACLK) begin
        if (!AXIS_ARESETN) 
        begin
            M_AXIS_TVALID <= 0;
            M_AXIS_TDATA <= 0;

            state <= S0;
        end
        else begin
            if (S_AXIS_TVALID && S_AXIS_TREADY) begin
                tx_buffer <= S_AXIS_TDATA[WIDTH-1 : 0];
                last <= S_AXIS_TLAST;
                spi_ready <= 0;
            end
            
            rx_full <= rx_full << 1;
            M_AXIS_TVALID <= rx_full[0] & ~rx_full[1];
    
            clk_counter <= clk_counter+1;
            if (clk_counter == CLOCK_DIVISOR) begin
                case(state)
                    S0:     begin
                                mosi <= 0;
                                rx_full[0] <= 0;

                                if (!spi_ready) begin
                                    mosi <= tx_buffer[WIDTH-1];
                                    last_buff <= last;
                                    counter <= 1;

                                    state <= S1;
                                end
                            end
                    S1:     begin
                                if (counter != WIDTH) begin
                                    tx_buffer <= tx_buffer << 1;   // clock phase = 0: data changes on trailing edge, sampling on leading edge
                                end else begin
                                   spi_ready <= 1;
                                end

                                rx_buffer = (rx_buffer << 1) | miso;
                                rx_full[0] <= 0;

                                state <= S2;
                            end
                    S2:     begin
                                mosi <= tx_buffer[WIDTH-1];

                                if (counter == WIDTH) begin
                                    M_AXIS_TDATA[WIDTH-1 : 0] = rx_buffer;
                                    rx_full[0] <= 1;
                                    if (!spi_ready && !last_buff) begin
                                        last_buff <= last;
                                        counter <= 1;
                                        
                                        state <= S1;
                                    end else begin
                                        state <= S0;
                                    end
                                end else begin
                                    counter <= counter + 1;
                                    
                                    state <= S1;
                                end
                            end
                    default:begin
                                state <= S0;
                            end
                endcase
            end
        end
    end

    endmodule
