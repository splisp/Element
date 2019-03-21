#include "engine/nodes/MidiProgramMapNode.h"

namespace Element {

MidiProgramMapNode::MidiProgramMapNode()
    : MidiFilterNode (0)
{
    jassert (metadata.hasType (Tags::node));
    metadata.setProperty (Tags::format, "Element", nullptr);
    metadata.setProperty (Tags::identifier, EL_INTERNAL_ID_MIDI_PROGRAM_MAP, nullptr);
    
}

MidiProgramMapNode::~MidiProgramMapNode() { }

void MidiProgramMapNode::clear()
{
    entries.clearQuick (true);
    ScopedLock sl (lock);
    for (int i = 0; i <= 127; ++i)
        programMap [i] = -1;
}

void MidiProgramMapNode::prepareToRender (double sampleRate, int maxBufferSize)
{
    ignoreUnused (sampleRate, maxBufferSize);
    
    {
        ScopedLock sl (lock);
        for (int i = 0; i <= 127; ++i)
            programMap [i] = -1;
        for (const auto* const entry : entries)
            programMap [entry->in] = entry->out;
    }
}

void MidiProgramMapNode::releaseResources() { }

void MidiProgramMapNode::render (AudioSampleBuffer& audio, MidiPipe& midi)
{
    ignoreUnused (audio, midi);
    if (midi.getNumBuffers() <= 0)
    {
        if (! assertedLowChannels)
        {
            DBG("[EL] PGC map: num bufs: " << midi.getNumBuffers());
            assertedLowChannels = true;
        }

        return;
    }
   
    auto* const midiIn = midi.getWriteBuffer (0);

    ScopedLock sl (lock);
    MidiMessage msg; int frame = 0;

    if (! toSendMidi.isEmpty())
    {
        MidiBuffer::Iterator iter1 (toSendMidi);
        while (iter1.getNextEvent (msg, frame))
            midiIn->addEvent (msg, frame);
        toSendMidi.clear();
    }

    MidiBuffer::Iterator iter2 (*midiIn);
    int program = -1;

    while (iter2.getNextEvent (msg, frame))
    {
        if (msg.isProgramChange() && programMap [msg.getProgramChangeNumber()] >= 0)
        {
            program = msg.getProgramChangeNumber();
            tempMidi.addEvent (MidiMessage::programChange (
                msg.getChannel(), programMap [msg.getProgramChangeNumber()]),
                frame);
        }
        else
        {
            tempMidi.addEvent (msg, frame);
        }
    }

    if (program >= 0 && program != lastProgram)
    {
        lastProgram = program;
        triggerAsyncUpdate();
    }

    midiIn->swapWith (tempMidi);
    traceMidi (*midiIn);
    tempMidi.clear();
}

void MidiProgramMapNode::sendProgramChange (int program, int channel)
{
    const auto msg (MidiMessage::programChange (channel, program));
    ScopedLock sl (lock);
    toSendMidi.addEvent (msg, 0);
}

int MidiProgramMapNode::getNumProgramEntries() const { return entries.size(); }

void MidiProgramMapNode::addProgramEntry (const String& name, int programIn, int programOut)
{
    if (programIn < 0)      programIn = 0;
    if (programIn > 127)    programIn = 127;
    if (programOut < 0)     programOut = programIn;
    if (programOut > 127)   programOut = 127;
    
    ProgramEntry* entry = nullptr;
    for (auto* e : entries)
    {
        if (e->in == programIn)
        {
            entry = e;
            break;
        }
    }

    if (entry == nullptr)
        entry = entries.add (new ProgramEntry());

    jassert (entry != nullptr);

    entry->name = name;
    entry->in   = programIn;
    entry->out  = programOut;
    sendChangeMessage();

    ScopedLock sl (lock);
    programMap [entry->in] = entry->out;
}

void MidiProgramMapNode::editProgramEntry (int index, const String& name, int inProgram, int outProgram)
{
    if (auto* entry = entries [index])
    {
        entry->name     = name.isNotEmpty() ? name : entry->name;
        entry->in       = inProgram;
        entry->out      = outProgram;
        ScopedLock sl (lock);
        programMap[entry->in] = entry->out;
        sendChangeMessage();
    }
}

void MidiProgramMapNode::removeProgramEntry (int index)
{
    std::unique_ptr<ProgramEntry> deleter;
    if (auto* const entry = entries [index])
    {
        entries.remove (index, false);
        deleter.reset (entry);
        ScopedLock sl (lock);
        programMap[entry->in] = -1;
        sendChangeMessage();
    }
}

MidiProgramMapNode::ProgramEntry MidiProgramMapNode::getProgramEntry (int index) const
{
    if (auto* const entry = entries [index])
        return *entry;
    return { };
}

}