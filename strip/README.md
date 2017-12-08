# spindle-strip

This is an RDF _processor_ module for
[Twine](https://github.com/bbcarchdev/twine). Its purpose is to drop undesired
triples from the set of graphs being processed.

## Operation

The module loops over every input triple, checking if the predicate is in a
[white list](#white-list) built from aspects of the [Spindle
rule-base](https://github.com/bbcarchdev/spindle/tree/develop/correlate#rule-base).

Any triple whose predicate is not in the white list is dropped from the graph
set before the next Twine processing step.

If an error occurs at any point during processing by `spindle-strip`, the whole
set of graphs is passed through the module unchanged.

## White list

Given rule-base triples with any of the following forms:

* `<P> a spindle:Property`;
* `<P> spindle:property <X>`;
* `<P> spindle:inverseProperty <X>`;
* `<X> spindle:expressedAs <P>` where `P` is not a resource; or
* `<P> spindle:coref <match-type>` for valid match types; then

`P` is a predicate URI that is added to the white list. `X` can be anything, and
a "match type" is one of the `spindle-correlate` predicates indicating what sort
of co-reference matching should occur, e.g. `spindle:resourceMatch`. In the
final case, `P` will also be added if there are no available match types (e.g.
if `spindle-correlate` was not loaded).

Additionally, two predicates are hard-coded to always be present in the white
list, `rdf:type` and `owl:sameAs`.
