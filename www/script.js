$( document ).ready(function() {
    getList();
    getData();
});

function getData() {

    $( ".radek" ).empty();

    url = "getData.php";

    day = $( "#list" ).val();

    $.post( url , { den: day}, function( data ) {                 
        $( ".radek" ).append( data );
    });
}



function getList(){
    url="getList.php";
    $.get( url , function( data ) {
                    
    $( "#list" ).append( data );
    });

}