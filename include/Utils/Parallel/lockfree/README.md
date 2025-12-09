
### Single-producer single-consumer data structures

- **Queue** - Best for single element operations, extremely fast, simple API consisting of only 2 methods.
- **Ring Buffer** - A more general data structure with the ability to handle multiple elements at a time, uses standard library copies making it very fast for bulk operations.
- **Bipartite Buffer** - A variation of the ring buffer with the ability to always provide linear space in the buffer, enables in-buffer processing.
- **Priority Queue** - A Variation of the queue with the ability to provide different priorities for elements, very useful for things like signals, events and communication packets.

These data structures are more performant and should generally be used whenever there is only one thread/interrupt pushing data and another one retrieving it.

### Multi-producer multi-consumer data structures

- **Queue** - Best for single element operations, extremely fast, simple API consisting of only 2 methods.
- **Priority Queue** - A Variation of the queue with the ability to provide different priorities for elements, very useful for things like signals, events and communication packets.
