#!/usr/bin/env fish

# 1. Define the list of files exactly as provided
set raw_files \
    ../main.cpp \
    ../socketSend.cpp \
    ../socketReceive.cpp \
    ../layoutWindow.cpp \
    ../tools.cpp \
    ../DataType.cpp \
    ../checkTerminal.cpp \
    ../terminal.cpp \
    ../terminal.hpp \
    ../checkTerminal.cpp \
    ../checkTerminal.hpp \
    ../producer.cpp \
    ../videoDecoder.cpp \
    ../shm.cpp \
    ../gui.cpp \
    ../consumer.cpp \
    ../renderer.cpp \
    ../base64converter.cpp \
    ../terminalLayout.cpp \
    ../consumerLayout.cpp

# 2. Array to keep track of already printed files to prevent duplicates
set printed_files

# Run the loop and redirect all of its combined output into stuff.txt
for raw_file in $raw_files
    # Clean the path: remove the leading '../'
    set file (string replace -r '^\.\./' '' $raw_file)

    # Skip if we have already processed this file
    if contains $file $printed_files
        continue
    end

    # Write the current file if it exists in the root directory
    if test -f $file
        echo "========================================="
        echo " FILE: $file"
        echo "========================================="
        cat $file
        echo ""
        
        # Mark as printed
        set -a printed_files $file
    end

    # If it's a .cpp file, check for its corresponding .hpp companion
    if string match -q '*.cpp' $file
        set hpp_file (string replace -r '\.cpp$' '.hpp' $file)

        # Write the .hpp file if it exists and hasn't been processed yet
        if test -f $hpp_file
            if not contains $hpp_file $printed_files
                echo "========================================="
                echo " FILE: $hpp_file (Auto-located)"
                echo "========================================="
                cat $hpp_file
                echo ""
                
                # Mark as printed
                set -a printed_files $hpp_file
            end
        end
    end
end > stuff.txt

# Print a nice confirmation to the terminal screen
echo "Done! Codebase compiled into stuff.txt"