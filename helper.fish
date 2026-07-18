#!/usr/bin/env fish

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
    ../consumerLayout.cpp \
    ../consumerRegistry.cpp \
    ../hyprlandIPC.cpp \
    ../layoutSHM.cpp

set printed_files

for raw_file in $raw_files
    set file (string replace -r '^\.\./' '' $raw_file)

    if contains $file $printed_files
        continue
    end

    if test -f $file
        echo "========================================="
        echo " FILE: $file"
        echo "========================================="
        cat $file
        echo ""
        
        # Mark as printed
        set -a printed_files $file
    end

    if string match -q '*.cpp' $file
        set hpp_file (string replace -r '\.cpp$' '.hpp' $file)

        if test -f $hpp_file
            if not contains $hpp_file $printed_files
                echo "========================================="
                echo " FILE: $hpp_file (Auto-located)"
                echo "========================================="
                cat $hpp_file
                echo ""
                
                set -a printed_files $hpp_file
            end
        end
    end
end > stuff.txt

echo "Done! Codebase compiled into stuff.txt"