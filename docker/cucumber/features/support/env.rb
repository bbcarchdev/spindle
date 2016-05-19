require 'net/http'
require 'openssl'
require 'rdf'
require 'rdf/ntriples'

def count(uri)
        graph = RDF::Graph.load(uri)

        work = RDF::URI.new("http://purl.org/vocab/frbr/core#Work")
        person = RDF::URI.new("http://xmlns.com/foaf/0.1/Person")

        n = 0
        graph.query([nil, RDF.type, work]) do |statement|
          n += 1
        end
        graph.query([nil, RDF.type, person]) do |statement|
          #puts "#{statement.subject.inspect}"
          n += 1
        end

        return n
end
