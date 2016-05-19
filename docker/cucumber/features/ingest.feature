#encoding: utf-8
Feature: Ingesting nquad files into Twine/Spindle

Scenario: Ingesting Shakespeare collection sample
	When "shakespeare-sample.nq" is ingested into Twine
	And I count the amount of persons and creative works that are ingested
	And A collection exists for "http://shakespeare.acropolis.org.uk/#id"
	Then The number of persons and creative works in the collection should be the same
