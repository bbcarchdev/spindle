# spindle-generate

This plug-in for Twine is responsible for examining a set of source triples
about some entity (which has already been processed by spindle-correlate) and:

* using the rule-base, generating the data stored as the 'proxy' entity; and
* storing some of that data in a relational database, if configured to do so, for later querying.

For example, the rule-base might indicate that an rdfs:label triple be
generated from proxies, derived from the various different kinds of labelling
predicate which might appear in source data (dc:title, dct:title, rdfs:label
itself, and so on).

If successfully derived, rdfs:label triples would be generated where the
locally-defined 'proxy' URI is the subject, and the derived version of the
label is the object.

In other words, `spindle-generate` generates triples which reflect Spindle's
understanding of a particular entity, based upon the content of the rule-base.

There are two purposes to this: one is to allow querying of those properties
in a unified fashion; the second is to enable applications not to have to do
this work themselves unless they need to because Spindle's rule-base is not
completely aligned with their specific needs.

