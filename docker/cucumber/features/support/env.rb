require 'net/http'
require 'openssl'
require 'rdf'
require 'rdf/ntriples'

def count(uri)
        graph = RDF::Graph.load(uri)
        objs = [
                RDF::URI.new("http://purl.org/vocab/frbr/core#Work"),
                RDF::URI.new("http://xmlns.com/foaf/0.1/Person"),
                RDF::URI.new("http://shakespeare.acropolis.org.uk/ontologies/work#Play"),
        ]

        n = 0
        objs.each do |obj|
                graph.query([nil, RDF.type, obj]) do |st|
                        n += 1
                end
        end
        puts "(#{n} entities found)"
        return n
end
